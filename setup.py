from setuptools import setup, Extension

yoink_builtin = Extension('yoink_builtin', ['src/yoink_builtin.c'])

setup(name='yoink_builtin', ext_modules=[yoink_builtin])
