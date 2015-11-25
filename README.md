# ITTIA DB SQL Embedded Database C API Examples

This project contains examples for the C API of the ITTIA DB SQL embedded database.


# Requirements

An [ITTIA DB SQL evaluation kit][1] is required to build the examples.

These examples, like ITTIA DB SQL itself, are cross-platform and can be compiled with any standard C/C++ compiler. A GNU makefile and Visual Studio project files are included with the examples.

# Building the Examples

## Windows 32-bit

 1. Install [ITTIA DB SQL evaluation kit][1] for 32-bit Windows.
 2. Copy `C:\Program Files (x86)\ITTIA DB SQL\*\msvc2010` into this folder and rename the folder to `ittiadb`.
 3. Open `build\vs*\api-examples-c.sln` in Visual Studio.
 4. Select the `Win32` platform.
 5. Build the solution.

## Windows 64-bit

 1. Install [ITTIA DB SQL evaluation kit][1] for 64-bit Windows.
 2. Copy `C:\Program Files\ITTIA DB SQL\*\msvc2012_64` into this folder and rename the folder to `ittiadb`.
 3. Open `build\vs*\api-examples-c.sln` in Visual Studio.
 4. Select the `x64` platform.
 5. Build the solution.

## Linux

 1. Extract [ITTIA DB SQL evaluation kit][1] for Linux to the current directory.
 2. Run:

        make ITTIA_DB_HOME="$PWD/opt/ittiadb"

If you extract the evaluation kit to the root directory instead, `make` can be run with no arguments.

[1]: http://www.ittia.com/products/embedded/evaluation
