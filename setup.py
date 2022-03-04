from setuptools import setup, Extension

cripple_builtin = Extension('cripple_builtin', ['src/cripple_builtin.c'])

setup(name='cripple_builtin', ext_modules=[cripple_builtin])
