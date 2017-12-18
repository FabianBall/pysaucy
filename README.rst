pysaucy
=======
A Python binding for the saucy algorithm for the graph automorphism problem

Install
-------
To install, run ``python setup.py install`` and ensure the source code of
`Saucy <http://vlsicad.eecs.umich.edu/BK/SAUCY/>`_ is found in the
path ``./saucy``.
The source code is available from the authors on request.

Documentation
-------------
The documentation can be found under https://fabianball.github.io/pysaucy/html/

Changes
-------
0.3.1
  - Faster orbit partition
  - Karate graph used for test

0.3
  - Removed the use of global static variables

0.2.2
  - Added support for initially colored partition

0.2.1
  - Added more tests
  - Added doc strings
  - Added Sphinx documentation support

0.2
  - Removed old debug stuff
  - Added code comments
  - Added computation and return of the orbit partition
  - Many small fixes

0.1.1
  - Fix of a severe bug where not enough memory was allocated
  - Small fixes

0.1
  - First version
