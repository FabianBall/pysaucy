#include "saucywrap.h"

/*
 * A saucy data struct which holds all data that is needed during the execution of nauty
 * when a new generator is found.
 *
 * py_callback -> reference to a Python function or Py_None and will be called if available
 * py_graph -> reference to the 'original' Python graph
 * orbit_partition -> array of size n that holds the orbit id for each node. Gets successively updated.
 */
struct saucy_data {
    PyObject *py_callback;
    PyObject *py_graph;
    int *orbit_partition;
};


/*
 * Initialize the saucy data that is passed as parameter when calling saucy and
 * will be available within the on_automorphism callback.
 */
static struct saucy_data*
initialize_saucy_data(PyObject *py_graph, PyObject *py_callback, int n)
{
    int *orbits;
    struct saucy_data *data = malloc(sizeof(struct saucy_data));

    if (!data) {
        PyErr_SetString(PyExc_MemoryError, "Memory for saucy data could not be allocated.");
        return NULL;
    }

    orbits = malloc(n * sizeof(int));

    if (!orbits) {
        PyErr_SetString(PyExc_MemoryError, "Memory for orbit partition could not be allocated.");
        free(data);
        return NULL;
    } else {
        for (int i = 0; i < n; i++)
            orbits[i] = -1;
    }

    // Both Python objects are saved => increase reference count
    Py_INCREF(py_callback);
    Py_INCREF(py_graph);
    data->py_callback = py_callback;
    data->py_graph = py_graph;
    data->orbit_partition = orbits;

    return data;
}

/*
 * Free the saucy data and release references to Python objects.
 */
static void
free_saucy_data(struct saucy_data *data)
{
    free(data->orbit_partition);
    
    // Decrease ref for the callback. It can be Py_None, 
    // but such references also need to be deremented
    Py_DECREF(data->py_callback);
    Py_DECREF(data->py_graph);

    free(data);
}

static void
graph_free(struct saucy_graph *g)
 {
    if (g) {
        free(g->adj);
        free(g->edg);
        free(g);
        g = NULL;
    }
}


/*
 * Safely convert a PyIntObject to a C signed int if possible.
 */
static int
PyInt_AsInt(PyObject *o)
{
    if (!PyInt_Check(o)) {
        PyErr_SetString(PyExc_TypeError, "Not a Python integer.");
        return -1;
    }

    int W = sizeof(int);
    long long MAX_INT = (1 << (W*8 - 1)) - 1;  // = 2**(wordlength - 1) - 1 (signed int)
    long py_int_value = PyInt_AsLong(o); // Already fails if too large

    if (py_int_value == -1 && PyErr_Occurred()) {
        return -1;
    } else if (py_int_value > MAX_INT) {
        PyErr_SetString(PyExc_ValueError, "Could not convert Python integer to C integer as it is too large.");
        return -1;
    } else {
        return (int)py_int_value;
    }
}

// Adapted from saucyio.c
/*
 * Converts array of values into array of cumulative sums, starting from 0.
 */
static void
shift_indices(int n, int *adj)
{
    int s = adj[0];
    int t, i;

    adj[0] = 0;
    for (i = 1; i < n; ++i) {
        t = adj[i];
        adj[i] = adj[i-1] + s;
        s = t;
    }
}

// Adapted from saucyio.c
/*
 * Converts array of values into array of cumulative sums, starting from 0.
 */
static void
rewind_indices(int n, int e, int *adj)
{
    int i;

	/* Translate again-broken sizes to adj values */
	for (i = n-1; i > 0; --i) {
		adj[i] = adj[i-1];
	}
	adj[0] = 0;
	adj[n] = e;
}


/*
 * Updates orbit ids for the given permutation.
 *
 * global_orbit must be an array of n orbit ids which is initially all -1
 * perm is a permutation in explicit form as returned by saucy
 */
static void
update_orbits(int *global_orbit, const int *perm, int n)
{
    int i, j, k, oid, old_oid;

    short *touched = calloc(n, sizeof(short));  // short suffices -> only boolean values

    for (i = 0; i < n; i++) {
        if (perm[i] == i) {  // i is fixed
            continue;
        } else if (touched[i]) {  // The cycle which i is on was already visited
                continue;
        } else {
            if (global_orbit[i] >= 0) {  // Already colored
                oid = global_orbit[i];
            } else {
                oid = i;
                global_orbit[i] = oid;
            }

            touched[i] = 1;  // Set the current node as touched to prevent iterating over the cycle a 2nd time

            j = perm[i];
            do {  // Color all nodes on the current cycle
                if (global_orbit[j] < 0) {  // Not colored yet
                    global_orbit[j] = oid;
                } else if (global_orbit[j] == oid) {  // Already on the same orbit
                    ; // NOOP
                } else {  // Already colored with another orbit id
                    old_oid = global_orbit[j];
                    for (k = 0; k < n; k++) {  // Re-color the nodes which have the 'old' orbit id
                        if (global_orbit[k] == old_oid) {
                            global_orbit[k] = oid;
                        }
                    }
                }

                touched[j] = 1;  // Set the current node as touched to prevent iterating over the cycle a 2nd time

                j = perm[j];
            } while (perm[j] != i);
        }
    }

    free(touched);
}

/*
 * Updates orbit ids for all nodes that are fixed by Aut(G) and therefore
 * have no orbit id yet (still -1)
 * The orbit id is set to the node id.
 */
static void
finalize_orbits(int *global_orbit, int n) {
    for (int i = 0; i < n; i++) {
        if (global_orbit[i] < 0)
            global_orbit[i] = i;
    }
}


/*
 * Callback for what should happen if an automorphism is found
 */
static int
on_automorphism(int n, const int *gamma, int k, int *support, void *arg)
{
    struct saucy_data *data = (struct saucy_data*)arg;

    update_orbits(data->orbit_partition, gamma, n);

    if (PyCallable_Check(data->py_callback)) {
        PyObject *permutation, *perm_support, *ret_val;
        Py_ssize_t i;

        permutation = PyList_New(n);
        perm_support = PyList_New(k);
        
        if (!permutation || !perm_support) {
            PyErr_SetString(PyExc_RuntimeError, "List could not be created.");
            Py_XDECREF(permutation);
            Py_XDECREF(perm_support);
            return 0;
        }

        for (i = 0; i < n; i++) {
            PyList_SetItem(permutation, i, PyInt_FromLong(gamma[i]));
        }

        for (i = 0; i < k; i++) {
            PyList_SetItem(perm_support, i, PyInt_FromLong(support[i]));
        }

        ret_val = PyObject_CallFunctionObjArgs(data->py_callback, data->py_graph, permutation, perm_support, NULL);

        // Get rid of those references, the callback function must deal with them if needed (aka save an own ref)
        // XXX: Do we need to Py_DECREF the lists' elements, too?
        // -> If yes: how can we know, there are no other references to the list, which would mean we "destroy" the 
        //    references of the elements in the list?!
        // -> If no: how do the elements' references get Py_DECREF'd?
        Py_DECREF(permutation);
        Py_DECREF(perm_support);

        if (!ret_val) {
            PyErr_SetString(PyExc_RuntimeError, "Failed calling the saucy on-automorphism callback.");
            return 0;
        } else if ((ret_val == Py_None) || PyObject_IsTrue(ret_val)) {
            Py_DECREF(ret_val);
            return 1;
        } else {
            Py_DECREF(ret_val);
            return 0;
        }
    } else {
        return 1;
    }
}

/*
 * Create a saucy graph from a Python graph object.
 */
static struct saucy_graph*
make_graph(PyObject *py_graph, int directed)
{
    struct saucy_graph *g;
    int *aout, *eout, *ain, *ein, to_node;
    PyObject *py_adjacency_list, *nn, *nm, *py_edge_list, *py_to_node;
    Py_ssize_t i, j, edge_list_length;

    nn = PyObject_GetAttrString(py_graph, "n");
    nm = PyObject_GetAttrString(py_graph, "m");

    if (!nn || !nm) {
        PyErr_SetString(PyExc_AttributeError, "Error accessing attribute.");
        return NULL;
    }

    g = malloc(sizeof(struct saucy_graph));

    if (!g) {
        PyErr_SetString(PyExc_MemoryError, "Could not allocate graph memory.");
        return NULL;
    }

    g->n = PyInt_AsInt(nn);
    g->e = PyInt_AsInt(nm);

    Py_DECREF(nn);
    Py_DECREF(nm);

    if (((g->n == -1) || (g->e == -1)) && PyErr_Occurred()) {
        free(g);
        return NULL;
    } else if (g->n == 0) {
        PyErr_SetString(PyExc_AttributeError, "Empty graph detected.");
        free(g);
        return NULL;
    }

    // Adapted from saucyio.c
    aout = calloc(directed ? (2*g->n+2) : (g->n+1), sizeof(int));
//	eout = malloc(directed ? 2 * g->e * sizeof(int) : g->e * sizeof(int));
	eout = malloc(2 * g->e * sizeof(int));

	if (!aout || !eout) {
	    PyErr_SetString(PyExc_MemoryError, "Could not allocate graph memory.");
	    free(g);
        return NULL;
	}

	g->adj = aout;
	g->edg = eout;

	// Adapted from saucyio.c
	// Creates two new references for the incoming edges
    ain = aout + (directed ? (g->n+1) : 0);
	ein = eout + (directed ? g->e : 0);

    py_adjacency_list = PyObject_GetAttrString(py_graph, "adjacency_list");

    if (!py_adjacency_list) {
        PyErr_SetString(PyExc_AttributeError, "Error accessing adjacency list.");
        graph_free(g);
        return NULL;
    } else if (!PySequence_Check(py_adjacency_list)) {
        PyErr_SetString(PyExc_TypeError, "Adjacency list must implement the sequence protocol.");
        graph_free(g);
        Py_DECREF(py_adjacency_list);
        return NULL;
    }

    Py_ssize_t const n = PySequence_Length(py_adjacency_list);

    if ((n != g->n)) {
        PyErr_SetString(PyExc_AttributeError,
            "Graph consistency error: Adjacency list length does not equal parameter 'n'.");
        graph_free(g);
        Py_DECREF(py_adjacency_list);
        return NULL;
    }

    /* Count the size of each adjacency list */
    for (i = 0; i < n; i++) {
        py_edge_list = PySequence_GetItem(py_adjacency_list, i);

        if (!py_edge_list) {
            PyErr_SetString(PyExc_KeyError, "Error accessing edge list.");
            graph_free(g);
            Py_DECREF(py_adjacency_list);
            return NULL;
        } else if (!PySequence_Check(py_edge_list)) {
            PyErr_SetString(PyExc_TypeError, "Edge list for a single node must implement the sequence protocol.");
            graph_free(g);
            Py_DECREF(py_adjacency_list);
            Py_DECREF(py_edge_list);
            return NULL;
        }

        // Increase counter on how many edges start in node i
        edge_list_length = PySequence_Length(py_edge_list);
        aout[i] += edge_list_length;

        // Increase counter on how many edges end in nodes j (coming from i)
        for (j = 0; j < edge_list_length; j++) {
            py_to_node = PySequence_GetItem(py_edge_list, j);

            if (!py_to_node) {
                PyErr_SetString(PyExc_KeyError, "Could not get node from edge list.");
                graph_free(g);
                Py_DECREF(py_adjacency_list);
                Py_DECREF(py_edge_list);
                return NULL;
            }

            to_node = PyInt_AsInt(py_to_node);
            Py_DECREF(py_to_node);

            if (to_node == -1 && PyErr_Occurred()) {
                graph_free(g);
                Py_DECREF(py_adjacency_list);
                Py_DECREF(py_edge_list);
                return NULL;
            } else if (to_node >= n) {
                PyErr_SetString(PyExc_ValueError, "Invalid node id.");
                graph_free(g);
                Py_DECREF(py_adjacency_list);
                Py_DECREF(py_edge_list);
                return NULL;
            }

            ++ain[to_node];
        }

        Py_DECREF(py_edge_list);
    }

    // Shift values
    shift_indices(n, aout);
    if (directed) {
        shift_indices(n, ain);
    }

	/* Insert adjacencies */
	// We do not perform any type checks as they were performed above so there should be no problem here...
    for (i = 0; i < n; i++) {
        py_edge_list = PySequence_GetItem(py_adjacency_list, i);

        if (!py_edge_list) {
            PyErr_SetString(PyExc_KeyError, "Error accessing edge list.");
            graph_free(g);
            Py_DECREF(py_adjacency_list);
            return NULL;
        }

        for (j = 0; j < PySequence_Length(py_edge_list); j++) {
            py_to_node = PySequence_GetItem(py_edge_list, j);

            if (!py_to_node) {
                PyErr_SetString(PyExc_KeyError, "Could not get node from edge list.");
                graph_free(g);
                Py_DECREF(py_adjacency_list);
                Py_DECREF(py_edge_list);
                return NULL;
            }

            // We can be sure the conversion works as it already worked above!
            to_node = PyInt_AsInt(py_to_node);
            Py_DECREF(py_to_node);

            eout[aout[i]++] = to_node;
		    ein[ain[to_node]++] = i;
        }

        Py_DECREF(py_edge_list);
    }

    Py_DECREF(py_adjacency_list);

    if (directed) {
		rewind_indices(g->n, g->e, aout);
		rewind_indices(g->n, g->e, ain);
	}
	else {
		rewind_indices(g->n, 2 * g->e, aout);
	}

    return g;
}


/*
 * Initialize the node color partition which is a parameter for saucy.
 *
 * py_colors must be some Python sequence of length n (both checked)
 * or could be Py_None. Then the partition is all zeroes.
 */
static int*
initialize_color_partition(PyObject *py_colors, int n)
{
    PyObject *item;
    int *colors = calloc(n, sizeof(int));

    if (!colors) {
        PyErr_SetString(PyExc_MemoryError, "Saucy color partition memory could not be allocated.");

        return NULL;
    }

    if (py_colors == Py_None) {
        // Do nothing, we're fine
    } else if (PySequence_Check(py_colors) && PySequence_Length(py_colors) == n) {
        // Fill colors
        for (Py_ssize_t i = 0; i < n; i++) {
            item = PySequence_GetItem(py_colors, i);

            if (!item) {
                PyErr_SetString(PyExc_TypeError, "Could not retrieve item from color partition.");
                free(colors);

                return NULL;
            }

            colors[i] = PyInt_AsInt(item);
            Py_DECREF(item);

            // TODO: Color partition must not have colors >= n and colors be increasing numbers from 0!
            if (colors[i] < 0) {
                if (!PyErr_Occurred()) {
                    PyErr_SetString(PyExc_ValueError, "Negative colors are not allowed.");
                }

                free(colors);

                return NULL;
            }
        }

    } else {
        PyErr_SetString(PyExc_TypeError, "The color partition must be a sequence of integers of length n.");
        free(colors);

        return NULL;
    }

    return colors;
}


/*
 * Make the saucy call on the given graph, with the given color partition,
 * and the given saucy data.
 */
static struct saucy_stats*
execute_saucy(struct saucy_graph *g, int directed, int *colors, struct saucy_data *data)
{
    struct saucy *s;
    struct saucy_stats *stats;

    s = saucy_alloc(g->n);

    if (!s) {
        PyErr_SetString(PyExc_MemoryError, "Saucy memory could not be allocated.");
        return NULL;
    }

    stats = malloc(sizeof(struct saucy_stats));

    if (!stats) {
        PyErr_SetString(PyExc_MemoryError, "Saucy memory could not be allocated.");
        saucy_free(s);
        return NULL;
    }

    saucy_search(s, g, directed, colors, on_automorphism, data, stats);

    // Cleanup
    saucy_free(s);

    return stats;
}

static char run_saucy_docs[] =
"run_saucy(g):\n\
    Run saucy for the given graph 'g' and \
    return (grpsize_base, grpsize_exp, levels, nodes, bads, gens, support) \
    as PyTuple.\n";

static PyObject*
run_saucy(PyObject *self, PyObject *args)
{
    PyObject *py_graph, *ret, *py_directed, *py_colors;
    struct saucy_graph *g;
    struct saucy_stats *stats;
    struct saucy_data *data;
    PyObject *py_callback;
    int directed, n;
    int *colors;

    // Parse arguments
    if (!PyArg_ParseTuple(args, "OOO", &py_graph, &py_callback, &py_colors)) {
        PyErr_SetString(PyExc_TypeError, "Function takes exactly three parameters.");
        return NULL;
    }

    // Check if arguments are not None
    if (py_graph == Py_None) {
        PyErr_SetString(PyExc_TypeError, "Argument must not be 'None'.");
        return NULL;
    }

    // Check if callback is actually callable
    if ((py_callback != Py_None) && !PyFunction_Check(py_callback)) {
        PyErr_SetString(PyExc_ValueError, "Argument must be callable.");
        return NULL;
    }

    // Duck-type style check if py_graph is a graph
    // FIXME: Is this enough??
    if (!(py_directed = PyObject_GetAttrString(py_graph, "directed"))) {
        PyErr_SetString(PyExc_ValueError, "Argument must be a graph instance.");
        return NULL;
    }

    // Obtain directedness
    directed = PyObject_IsTrue(py_directed);
    Py_DECREF(py_directed);

    //Create the graph
    g = make_graph(py_graph, directed);

    if (!g) {
        if (!PyErr_Occurred()) {
            PyErr_SetString(PyExc_TypeError, "The saucy graph could not be created.");
        }

        return NULL;
    }

    n = g->n;

    // Initialize the color partition
    colors = initialize_color_partition(py_colors, n);

    if (!colors) {
        graph_free(g);
        return NULL;
    }

    // Initialize saucy data
    data = initialize_saucy_data(py_graph, py_callback, n);

    if (!data) {
        graph_free(g);
        free(colors);
        return NULL;
    }

    // Finally execute saucy
    stats = execute_saucy(g, directed, colors, data);
    graph_free(g);
    free(colors);

    if (PyErr_Occurred()) {
        free(stats);
        free_saucy_data(data);
        return NULL;
    }

    finalize_orbits(data->orbit_partition, n);

    PyObject *orbits = PyList_New(n);

    if (!orbits) {
        PyErr_SetString(PyExc_RuntimeError, "List could not be created.");
        free(stats);
        free_saucy_data(data);
        return NULL;
    }

    for (Py_ssize_t j = 0; j < n; j++) {
        PyList_SetItem(orbits, j, Py_BuildValue("i", data->orbit_partition[j]));
    }

    free_saucy_data(data);

    ret = PyTuple_New(8);
    PyTuple_SET_ITEM(ret, 0, Py_BuildValue("d", stats->grpsize_base));
    PyTuple_SET_ITEM(ret, 1, Py_BuildValue("i", stats->grpsize_exp));
    PyTuple_SET_ITEM(ret, 2, Py_BuildValue("i", stats->levels));
    PyTuple_SET_ITEM(ret, 3, Py_BuildValue("i", stats->nodes));
    PyTuple_SET_ITEM(ret, 4, Py_BuildValue("i", stats->bads));
    PyTuple_SET_ITEM(ret, 5, Py_BuildValue("i", stats->gens));
    PyTuple_SET_ITEM(ret, 6, Py_BuildValue("i", stats->support));
    PyTuple_SET_ITEM(ret, 7, orbits);

    free(stats);

    return ret;
}

static PyMethodDef saucywrap_methods[] = {
    {"run_saucy", run_saucy, METH_VARARGS, run_saucy_docs},
    {NULL, NULL, 0, NULL}
};

PyMODINIT_FUNC
initsaucywrap(void)
{
    (void) Py_InitModule("saucywrap", saucywrap_methods);
}
