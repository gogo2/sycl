//==- partition_array.hpp --- SYCL Xilinx SSDM intrinsics            -------==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file defines some of the SSDM intrinsics used in Xilinx tools.
///
//===----------------------------------------------------------------------===//

#ifndef SYCL_XILINX_FPGA_SSDM_INST_HPP
#define SYCL_XILINX_FPGA_SSDM_INST_HPP

// This file defines some of the SSDM intrinsics used in Xilinx tools.
// The SSDM intrinsics and the SYCL extensions that rely on them should be a
// no-op on non-Xilinx devices and a no-op on the host
#if (defined(__SYCL_DEVICE_ONLY__) && defined(__SYCL_XILINX_ONLY__))
extern "C" {
  /// SSDM Intrinsics: dataflow operation
  void _ssdm_op_SpecDataflowPipeline(...)
    __attribute__ ((nothrow, noinline, weak));
  /// SSDM Intrinsics: pipeline operation
  void _ssdm_op_SpecPipeline(...) __attribute__ ((nothrow, noinline, weak));
  /// SSDM Intrinsics: array partition operation
  void _ssdm_SpecArrayPartition(...) __attribute__ ((nothrow, noinline, weak));
}
#else
/* If not on device, just ignore the intrinsics as defining them as
   empty variadic macros replaced by an empty do-while to avoid some
   warning when compiling (and used in an if branch */
#define _ssdm_op_SpecDataflowPipeline(...) do { } while (0)
#define _ssdm_op_SpecPipeline(...) do { } while (0)
#define _ssdm_SpecArrayPartition(...) do { } while (0)
#endif

#endif // SYCL_XILINX_FPGA_SSDM_INST_HPP
