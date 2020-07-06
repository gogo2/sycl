//==- opt_decorate_func.hpp --- SYCL Xilinx extension         ---------------==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file includes some decorating functions for some optimizations supported
/// by Xilinx tools.
///
//===----------------------------------------------------------------------===//

#ifndef SYCL_XILINX_FPGA_OPT_DECORATE_FUNC_HPP
#define SYCL_XILINX_FPGA_OPT_DECORATE_FUNC_HPP

__SYCL_INLINE_NAMESPACE(cl) {

namespace sycl::xilinx {

/** Apply dataflow execution on functions or loops

    With this mode, Xilinx tools analyze the dataflow dependencies
    between sequential functions or loops and create channels (based
    on ping-pong RAMs or FIFOs) that allow consumer functions or loops
    to start operation before the producer functions or loops have
    completed.

    This allows functions or loops to operate in parallel, which
    decreases latency and improves the throughput.

    \param[in] f is a function that functions or loops in f will be executed
    in a dataflow manner.
*/

template <typename T>
void dataflow(T functor) {
  _ssdm_op_SpecDataflowPipeline(-1, "");
  functor();
}

/** Execute loops in a pipelined manner

    A loop with pipeline mode processes a new input every clock
    cycle. This allows the operations of different iterations of the
    loop to be executed in a concurrent manner to reduce latency.

    \param[in] f is a function with an innermost loop to be executed in a
    pipeline way.
*/
template <typename T>
void pipeline(T functor) {
  _ssdm_op_SpecPipeline(1, 1, 0, 0, "");
  functor();
}

}

}

#endif // SYCL_XILINX_FPGA_OPT_DECORATE_FUNC_HPP
