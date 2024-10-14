# QI-QMP
QI-QMP is a minuscule C++/Qt library that provides an interface to QEMU instances via the QEMU Machine Protocol. In other words, this library implements the client side of QMP for C++.

It presents as a QObject derived class, `Qmpi`, that is operated via member functions and connecting to its signals.

This project makes use of the CMake build system generator for both compilation and consumption of the library.

It is based on Qt 6.

[![Dev Builds](https://github.com/oblivioncth/QI-QMP/actions/workflows/build-project.yml/badge.svg?branch=dev)](https://github.com/oblivioncth/QI-QMP/actions/workflows/build-project.yml)

## Documentation:
Detailed documentation of this library, facilitated by Doxygen, is available at: https://oblivioncth.github.io/QI-QMP/

## Getting Started
Either grab the latest [release](https://github.com/oblivioncth/QI-QMP/releases/) or [build the library from source](https://oblivioncth.github.io/QI-QMP/index.html#autotoc_md4), and import using CMake.

Building from source is recommended as this library can easily be integrated as a dependency into your project using CMake's FetchContent. An example of this is demonstrated in the documentation.

Finally, the [Minimal Example](https://oblivioncth.github.io/QI-QMP/index.html#autotoc_md3), gives a basic overview of how to use the interface.

### Summary

 - C++20
 - CMake 3.23.0

### Dependencies
- Qt6
- [Qx](https://github.com/oblivioncth/Qx/)
- [OBCMake](https://github.com/oblivioncth/OBCmake) (build script support, fetched automatically)
- [Doxygen](https://www.doxygen.nl/)  (for documentation)

## Pre-built Releases/Artifacts

Releases and some workflows currently provide builds of QI-QMP in various combinations of platforms and compilers. View the repository [Actions](https://github.com/oblivioncth/QI-QMP/actions) or [Releases](https://github.com/oblivioncth/QI-QMP/releases) to see examples.

For all builds, Qt was configured as follows (excluding defaults):

 - Release
 - Compiler
    - Windows: win32-msvc
    - Linux: linux-clang
 - Shared/Static Linkage
 - Modules: qtbase, qtimageformats, qtnetworkauth, qtsvg, qt5compat
 - Features: relocatable
 - -ssl (Linux) / -schannel (Windows)
