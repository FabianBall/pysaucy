from setuptools import setup, Extension

import os

if not os.access('saucy', os.R_OK | os.X_OK):
    print('Error: Please make sure to have saucy sources available in the "saucy" directory')
    raise SystemExit(1)

saucywrap = Extension('pysaucy.saucywrap',
                      sources=['pysaucy/saucywrap.c'],
                      extra_compile_args=['-fPIC', '-O3'],
                      extra_link_args=['-fPIC'],
                      # Docs say: without extension, but omitting it breaks the build
                      extra_objects=['saucy/saucy.c', 'saucy/saucy.h'],
                      include_dirs=['saucy'],
                      )


setup(
    name='pysaucy',
    version='0.3.3b0',
    package_dir={'pysaucy': 'pysaucy'},
    packages=['pysaucy', 'pysaucy.tests'],
    url='https://github.com/FabianBall/pysaucy',
    license='MIT',
    author='Fabian Ball',
    author_email='fabian.ball@kit.edu',
    description='A Python binding for the saucy algorithm for the graph automorphism problem.',
    requires=['future'],
    ext_modules=[saucywrap],
    test_suite='nose.collector',
    tests_require=['nose'],
    include_package_data=True,
)
