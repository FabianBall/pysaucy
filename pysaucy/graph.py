"""

.. moduleauthor:: Fabian Ball <fabian.ball@kit.edu>
"""
from __future__ import absolute_import

from . import saucywrap


class Graph(object):
    """

    """
    def __init__(self, adjacency_list, directed=False):
        """
        The *adjacency_list* must have length n (number of nodes) and ``adjacency_list[i]`` contains
        an iterable of node ids to which an edge exists.

        If *directed* is False, the adjacency list will be transformed so that for each node id
        i holds ``i <= min(adjacency_list[i])``. I.e. only the right upper triangular matrix is
        saved.

        If *directed* is True, each edge in the adjacency list will be interpreted as directed edge.

        .. hint::
           Saucy natively supports loops in graphs. So having loops simply means that
           ``i in adjacency_list[i]`` is True.
           You can check if the graph has loops using the corresponding property.

        :param adjacency_list: An iterable of *n* entries, each an iterable of adjacent node ids
        :param directed: (Optional) Does the adjacency list describe a directed graph?
        """
        self._adjacency_list = list(adjacency_list)
        self._directed = directed
        self._loops = False

        self._check_adjacency_list()

    def _check_adjacency_list(self):
        """
        Check the adjacency list and do some normalizations:
        a) Ensure every entry is itself a list (edge list)
        b) Check for loops
        c) Remove values from the left lower triangular matrix for undirected graphs
        """

        # a)
        for i in range(len(self._adjacency_list)):
            self._adjacency_list[i] = list(self._adjacency_list[i])

        # b)
        for row_idx, row in enumerate(self._adjacency_list):
            while row_idx in row:
                # row.remove(row_idx)
                # warnings.warn('Loops are not allowed! Removed edge ({0}, {0})'.format(row_idx))
                self._loops = True
                break

            if self._loops:
                break

        # c)
        if not self.directed:
            # Keep only the right upper triangular matrix
            for row_idx in range(len(self._adjacency_list)):
                for col_idx in list(self._adjacency_list[row_idx]):
                    if col_idx <= row_idx:
                        self._adjacency_list[row_idx].remove(col_idx)  # Remove node index

                        # Add to row
                        if row_idx not in self._adjacency_list[col_idx]:
                            self._adjacency_list[col_idx].append(row_idx)

    @property
    def n(self):
        """
        Number of nodes :math:`n`

        :return: :math:`n`
        """
        return len(self._adjacency_list)

    @property
    def m(self):
        """
        Number of edges :math:`m`

        :return: :math:`m`
        """
        return sum(len(d) for d in self.adjacency_list)
        # if self._directed:
        #     return sum(len(d) for d in self.adjacency_list)
        # else:
        #     return 2 * sum(len(d) for d in self.adjacency_list)

    @property
    def directed(self):
        """
        Is this graph directed?

        :return: True | False
        """
        return self._directed

    @property
    def adjacency_list(self):
        """
        Return the (possibly transformed, see :func:`pysaucy.graph.Graph.__init__`)
        adjacency list.

        :return: List of length :math:`n` of lists of adjacent node ids.
        """
        return self._adjacency_list

    @property
    def has_loops(self):
        """
        Has the graph loops?

        :return: True | False
        """
        return self._loops


def run_saucy(g, colors=None, on_automorphism=None):
    """
    Make the saucy call.

    .. warning::
       Using the *on_automorphism* callback is quite expensive and will slow down the algorithm
       significantly if many generators are found.

    The automorphism group size is defined by *group size base* :math:`b` and
    *group size exponent* :math:`e` as :math:`b\cdot10^e`.

    The returned *orbits* are a list of orbit ids where ``orbits[i]`` is some (integer) orbit id
    and all nodes on the same orbit have the same id.

    :param g: The graph
    :param colors: (Optional) A partition of node colors of length :math:`n`
    :param on_automorphism: An optional callback function with signature (graph, permutation, support)
    :return: (group size base, group size exponent, levels, nodes, bads, number of generators, support, orbits)
    """
    return saucywrap.run_saucy(g, on_automorphism, colors)
