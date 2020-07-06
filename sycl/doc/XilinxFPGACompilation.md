# Xilinx FPGA Compilation

This document aims to cover the key differences of compiling SYCL for Xilinx
FPGAs. Things like building the compiler and library remain the same but other
things like the compiler invocation for Xilinx FPGA compilation is a little
different. As a general rule of thumb we're trying to keep things as close as we
can to the Intel implementation, but in some areas were still working on that.

One of the significant differences of compilation for Xilinx FPGAs over the
ordinary compiler directive is that Xilinx devices require offline compilation
of SYCL kernels to binary before being wrapped into the end fat binary. The
offline compilation of these kernels is done by Xilinx's `xocc` compiler rather
than the SYCL device compiler itself in this case. The device compiler's job is
to compile SYCL kernels to a format edible by `xocc`, then take the output of
`xocc` and wrap it into the fat binary as normal.

The current Intel SYCL implementation revolves around SPIR-V while
Xilinx's `xocc` compiler can only ingest SPIR-df as an intermediate
representation. SPIR-df is LLVM IR with some SPIR decorations. It is
similar to the SPIR-2.0 provisional specification but does not
requires the LLVM IR version to be 3.4. It uses just the encoding of
the LLVM used, which explains the `-df` as "de-facto".

So a lot of our modifications revolve
around being the middle man between `xocc` and the SYCL device
compiler and runtime for the moment, they are not the simple whims of
the insane! Hopefully...

## Getting started guide using Ubuntu 19.04, SDx 2019.1 and Alveo U200

Look at [getting started with an Alveo U200](GettingStartedAlveo.md).

## Software requirements

Installing Xilinx FPGA compatible software stack:
  1.  OpenCL headers: On Ubuntu/Debian this can be done by installing the
      opencl-c-headers package, e.g. `apt install opencl-c-headers`.
      Alternatively the headers can be download from
      [github.com/KhronosGroup/OpenCL-Headers](https://github.com/KhronosGroup/OpenCL-Headers)
  2.  Xilinx runtime (XRT) for FPGAs: Download, build and install [XRT](https://github.com/Xilinx/XRT),
      this contains the OpenCL runtime.
  3.  Xilinx SDx (2018.3+): Download and Install [SDx](https://www.xilinx.com/support/download/index.html/content/xilinx/en/downloadNav/sdx-development-environments.html)
      which contains the `xocc` compiler.

## Platforms

It's of note that the SDx 2018.3 install comes with several platforms that do
not work with the SYCL compiler. Instead, you'll have to use one of the newer
boards, like the Alveo U250 (*xilinx_u250_xdma_201830_1*). This requires some
additional installation steps as it doesn't come packaged with the SDx download.

How to:
  1.  Download the Deployment and Development Shells from the
      [Alveo U250 getting started page](https://www.xilinx.com/products/boards-and-kits/alveo/u250.html#gettingStarted).
      In the following instructions we assume Debian/Ubuntu Linux, however RHEL
      and CentOS packages are also available.
  2.  1. Install the Deployment Shell: ``sudo apt install xilinx-u250-xdma-201830.1_<OS>.deb``
      2. Install the Development Shell: ``sudo apt install xilinx-u250-xdma-201830.1-dev_<OS>.deb``

If you have trouble installing these via the package manager (for example using
a newer distribution like Ubuntu 18.10) it's possible to extract the files and
manually install them. The directory structure of the package mimics the default
install locations on your system, e.g. */opt/xilinx/platforms*. If you choose the
extraction route then all you really require for emulation is the files inside
the Development Shell.

The main files required for emulation of a U250 board are found inside the
Development Shell under platforms.

The main files required for deployment to a U250 board are inside the Deployment
Shell.

This set of instructions should be applicable to other boards you wish to test,
you can search for boards via the [boards-and-kits](https://www.xilinx.com/products/boards-and-kits/)
page.

## Environment & Setup

For the moment this projects only been tested on Linux (Ubuntu 18.10), so for
now we shall only detail the minimum setup required in this context.

In addition to the required environment variables for the base SYCL
implementation specified in [GetStartedWithSYCLCompiler.md](GetStartedWithSYCLCompiler.md);
compilation and execution of SYCL on FPGAs requires the following:

To setup SDx for access to the `xocc` compiler the following steps are required:

```bash
export XILINX_SDX=/path_to/SDx/2018.3
PATH=$XILINX_SDX/bin:$XILINX_SDX/lib/lnx64.o:$PATH
```

To setup XRT for the runtime the following steps are required:

```bash
export XILINX_XRT=/path_to/xrt
export LD_LIBRARY_PATH=$XILINX_XRT/lib:$LD_LIBRARY_PATH
PATH=$XILINX_XRT/bin:$PATH
```

On top of the above you should specify emulation mode which indicates to the
compiler what it should compile for and to the runtime what mode it should
execute in. It's of note that you will likely encounter problems if the binary
was compiled with a different emulation mode than is currently set in your
environment (the runtime will try to do things it can't).

The emulation mode can be set as:

* `sw_emu` for software emulation, this is the simplest and quickest compilation
  mode that `xocc` provides.
* `hw_emu` for hardware emulation, this more accurately represents the hardware
  your targeting and does more detailed compilation and profiling. It takes
  extra time to compile and link.
* `hw` for actual hardware compilation, takes a significant length of time to
  compile for a specified device target.

The emulation mode can be specified as follows:

```bash
export XCL_EMULATION_MODE=sw_emu
```

Xilinx platform description, your available platforms (device) can be found in
SDx's platform directory. Specifying this tells both compilers the desired
platform your trying to compile for and the runtime the platform it should be
executing for.

```bash
export XILINX_PLATFORM=xilinx_u250_xdma_201830_1
```

Generate an emulation configuration file, this should be in the executable
directory or your path. It's again, important the emulation configuration fits
your compiled binary or you may encounter some trouble. If there is no
configuration file found, it will default to a basic configuration which works
in most cases, but doesn't reflect your ideal platform. XRT warns you in these
cases.

```bash
emconfigutil -f $XILINX_PLATFORM --nd 1
```

## C++ Standard

It's noteworthy that we've altered the SYCL runtime to be compiled using C++20,
so we advise compiling your source code with C++20 (`-std=c++2a`). Although, most
of the runtimes current features are C++11 compatible outside of the components
in the Xilinx vendor related directories. However, this is likely to change as
we're interested in altering the runtime with newer C++ features.

## Compiling a SYCL program

At the moment we only support one step compilation, so you can't easily compile
just the device side component and then link it to the host side component.

The compiler invocation for the `single_task_vector_add.cpp` example inside
the [simple_tests](../test/xocc_tests/simple_tests) folder looks like this:

```bash
$SYCL_BIN_DIR/clang++ -std=c++2a -fsycl \
  -fsycl-targets=fpga64-xilinx-unknown-sycldevice single_task_vector_add.cpp \
  -o single_task_vector_add -lOpenCL -I/opt/xilinx/xrt/include/
```

Be aware that compiling for FPGA is rather slow.

## Compiler invocation differences

By setting the `-fsycl-targets` to `fpga64-xilinx-unknown-sycldevice` you're 
telling  the compiler to use our XOCC Tools and compile the device side code 
for Xilinx FPGA.

This hasn't been tested with mutliple `-fsycl-targets` yet (e.g. offloading to 
both a Xilinx and Intel FPGA) and is unlikely to work, so it is advisable to 
stick to compiling for a single target at the moment.

The runtime makes use of some Xilinx XRT OpenCL extensions when compiling for
Xilinx FPGAs, as such you need to include the XRT include directory for the time
being as they do not get packaged with the regular OpenCL include directory for 
now. The default install location for this on Debian/Ubuntu is: `/opt/xilinx/xrt/include/`

## Tested with

* Ubuntu 18.10
* XRT 2018.3
* SDx 2018.3
* Alveo U250 Platform: xilinx_u250_xdma_201830_1

* Ubuntu 19.04
* XRT 2019.1
* SDx 2019.1
* Alveo U200 Platform: xilinx_u200_xdma_201830_2


## Extra Notes:
* The Driver ToolChain, currently makes some assumptions about the `SDx` 
  installation. For example, it assumes that `xocc` is inside SDx's bin folder 
  and that the lib folder containing `SPIR` builtins that kernels are linked 
  against are in a `/lnx64/lib` directory relative to the bin folders parent. 
  This can be seen and altered in `XOCC.cpp` if so desired. A future, aim is 
  to allow the user to pass arguments through the compiler to assign these if 
  the assumptions are false. However, in the basic 2018.3 release the standard 
  directory structure that is assumed is correct without alterations.
