THUNDERCRACKER
==============

This is the software toolchain for Sifteo's 2nd generation product.


Parts
-----

deps
  Outside dependencies, included for convenience.

docs
  Build files for the SDK documentation, as well as other miscellaneous
  platform documentation.

emulator
  The Thundercracker hardware emulator. Includes an accurate
  hardware simulation of the cubes, and the necessary glue to
  execute "sim" builds of master firmware in lockstep with this
  hardware simulation. Also includes a unit testing framework.

extras
  Various userspace programs which are neither full games nor included in
  the SDK as "example" code. This directory can be used for internal tools
  and toys which aren't quite up to our standards for inclusion in the SDK.

firmware
  Firmware source for cubes and master.

launcher
  Source for the "launcher" app, the shell which contains the game selector
  menu and other non-game functionality that's packaged with the system.
  This is compiled with the SDK as an ELF binary.

sdk
  Development kit for game software, running on the master.
  This directory is intended to contain only redistributable components.
  During build, pre-packaged binaries, built binaries, and built docs
  are copied here.

stir
  Our asset preparation tool, the Sifteo Tiled Image Reducer.

test
  Unit tests for firmware, SDK, and simulator.

tools
  Internal tools for SDK packaging, factory tests, etc.

vm
  Tools and documentation for the virtual machine sandbox that games execute
  in. Includes "slinky", the Sifteo linker and code generator for LLVM.


Operating System
----------------
  
The code here should all run on Windows, Mac OS X, or Linux. Right now
the Linux port is infrequently maintained, but in theory it should
still work. In all cases, the build is Makefile based, and we compile
with some flavor of GCC.


Build
-----

Running "make" in this top-level directory will by default build all
firmware as well as the SDK.

Various dependencies are required:

1. A build environment; make, shell, etc. Use MSYS on Windows.
2. GCC, for building native binaries. Should come with (1).
3. SDCC, a microcontroller cross-compiler used to build the cube firmware
4. gcc for ARM (arm-none-eabi-gcc), used to build master-cube firmware.
5. Python, for some of the code generation tools
6. Doxygen, to build the SDK documentation

Optional dependencies:

1. OpenOCD, for installing and debugging master firmware
2. The Python Imaging Library, used by other code generation tools

Most of these dependencies are very easy to come by, and your favorite
Linux distro or Mac OS package manager has them already. The ARM cross
compiler is usually more annoying to obtain. On Linux and Windows, the
CodeSourcery C++ distribution is preferred. On Mac OS, the following
script will automatically build a compatible toolchain for your machine:

   https://github.com/jsnyder/arm-eabi-toolchain
