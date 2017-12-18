"""

.. moduleauthor:: Fabian Ball <fabian.ball@kit.edu>
"""
from __future__ import absolute_import

import collections
from unittest import TestCase

import pysaucy


class TestWrapper(TestCase):

    def setUp(self):
        self.k1000 = pysaucy.examples.complete(1000)

    def test_k1000(self):
        result = pysaucy.run_saucy(self.k1000)
        self.assertTupleEqual(tuple(list(result)[:7]), (4.023872600770939, 2567, 1000, 3995, 0, 999, 1998))

    def test_k10(self):
        k10 = pysaucy.examples.complete(10)
        result = pysaucy.run_saucy(k10)
        self.assertTupleEqual(tuple(list(result)[:7]), (3.6287999999999996, 6, 10, 35, 0, 9, 18))

    def test_empty_k10(self):
        empty = pysaucy.Graph([[]]*10)
        result = pysaucy.run_saucy(empty)
        self.assertTupleEqual(tuple(list(result)[:7]), (3.6287999999999996, 6, 10, 35, 0, 9, 18))

    def test_null_graph(self):
        null = pysaucy.Graph([])
        with self.assertRaises(AttributeError) as e:
            pysaucy.run_saucy(null)

        self.assertIn('Empty graph detected.', e.exception.message)

    def test_wrong_parameters(self):
        g = pysaucy.Graph([])
        g._adjacency_list = None
        with self.assertRaises(AttributeError) as e:
            pysaucy.run_saucy(g)

        self.assertIn('Error accessing attribute.', e.exception.message)

        g._adjacency_list = 1
        with self.assertRaises(AttributeError) as e:
            pysaucy.run_saucy(g)

        self.assertIn('Error accessing attribute.', e.exception.message)

        g._adjacency_list = [["1", 2, 3], [""]]
        with self.assertRaises(TypeError) as e:
            pysaucy.run_saucy(g)

        self.assertIn('Not a Python integer.', e.exception.message)

        g._adjacency_list = [[1, 2, 3], []]
        with self.assertRaises(ValueError) as e:
            pysaucy.run_saucy(g)

        self.assertIn('Invalid node id.', e.exception.message)

        c_int = 2**31
        g._adjacency_list = [[c_int]]
        with self.assertRaises(ValueError) as e:
            pysaucy.run_saucy(g)

        self.assertIn('Could not convert Python integer to C integer as it is too large.', e.exception.message)

        c_int = 2 ** 31 - 1
        g._adjacency_list = [[c_int]]
        with self.assertRaises(ValueError) as e:
            pysaucy.run_saucy(g)

        self.assertIn('Invalid node id.', e.exception.message)

    def test_loops(self):
        # Loop, no automorphisms
        g1 = pysaucy.Graph([[0, 1], []])
        result = pysaucy.run_saucy(g1)
        self.assertEqual(result, (1.0, 0, 1, 1, 0, 0, 0, [0, 1]))

        # No loop, automorphisms
        g2 = pysaucy.Graph([[1], []])
        result = pysaucy.run_saucy(g2)
        self.assertEqual(result, (2.0, 0, 2, 3, 0, 1, 2, [0, 0]))

        # Loops, automorphisms
        g3 = pysaucy.Graph([[0, 1], [1]])
        result = pysaucy.run_saucy(g3)
        self.assertEqual(result, (2.0, 0, 2, 3, 0, 1, 2, [0, 0]))

    def test_color_partition(self):
        k11 = pysaucy.examples.complete(11)
        colors = [0]*11
        colors[10] = 1
        # colors 'fixes' one node by putting it into another partition
        # => Aut(k11, colors) ~ Aut(k10)
        result = pysaucy.run_saucy(k11, colors=colors)
        self.assertTupleEqual(tuple(list(result)[:7]), (3.6287999999999996, 6, 10, 35, 0, 9, 18))

    def test_karate(self):
        karate = pysaucy.examples.karate()
        result = pysaucy.run_saucy(karate)
        self.assertEqual(480, result[0] * 10**result[1])
        orbit_sizes = collections.defaultdict(int)
        for orbit_id in result[7]:
            orbit_sizes[orbit_id] += 1
        self.assertEqual(len(orbit_sizes), 27)
