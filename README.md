# QI-QMP
QI-QMP is a minuscule C++ library, which utilizes Qt, that provides an interface to QEMU instances via the QEMU Machine Protocol. In other words, this library implements the client side of QMP for C++.

It presents as a QObject derived class, `Qmpi`, that is operated via member functions and connecting to its signals.

This project makes use of the CMake build system generator for both compilation and consumption of the library.

It is based on Qt 6.

[![Dev Builds](https://github.com/oblivioncth/QI-QMP/actions/workflows/push-reaction.yml/badge.svg?branch=dev)](https://github.com/oblivioncth/QI-QMP/actions/workflows/push-reaction.yml)

## Documentation:
Detailed documentation of this library, facilitated by Doxygen, is available at: https://oblivioncth.github.io/QI-QMP/

## Getting Started
Either grab the latest [release](https://github.com/oblivioncth/QI-QMP/releases/) or [build the library from source](https://oblivioncth.github.io/QI-QMP/index.html#autotoc_md4), and import using CMake.

Building from source is recommended as this library can easily be integrated as a dependency into your project using CMake's FetchContent. An example of this is demonstrated in the documentation.

Finally, the [Minimal Example](https://oblivioncth.github.io/QI-QMP/index.html#autotoc_md3), gives a basic overview of how to use the interface.

### Summary

 - C++20
 - CMake 3.21.1

### Dependencies
- Qt6
- [Qx](https://github.com/oblivioncth/Qx/)

## Pre-built Releases/Artifacts

Releases and some workflows currently provide builds of Qx in the following configurations:

1) - Windows (windows-latest)
    - MSVC (latest)
    - Debug & Release
    - Static Linkage
    - Statically Linked Qt
>>
2) - Windows (windows-latest)
    - MSVC (latest)
	- Debug & Release
	- Static Linkage
	- Dynamically Linked Qt

>>
3) - Ubuntu (ubuntu-latest)
    - Clang-12
	- Debug & Release
	- Static Linkage
	- Dynamically Linked Qt
	
>>
4) - Ubuntu (ubuntu-latest)
    - Clang-12
	- Debug & Release
	- Static Linkage
	- Statically Linked Qt

For all builds, Qt was configured as follows (excluding defaults):

 - Release
 - Shared/Static Linkage
 - Modules: qtbase, qtimageformats, qtnetworkauth, qtsvg
 - Features: relocatable
 - -ssl (Linux) / -schannel (Windows)
