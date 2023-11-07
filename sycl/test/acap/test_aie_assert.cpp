// REQUIRES: acap

// RUN: %acap_clang %s -o %s.bin
// RUN: %add_acap_result %s.bin
// RUN: rm %s.bin

#include <sycl/sycl.hpp>

/// Example of using neighbor memory tiles
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

using namespace sycl::vendor::xilinx;
using namespace trisycl::vendor::xilinx;

/// A memory tile has to inherit from acap::aie::memory<AIE, X, Y>
template <typename AIE, int X, int Y>
struct tile_memory : acap::aie::memory<AIE, X, Y> {
};

template <typename AIE, int X, int Y> struct prog : acap::aie::tile<AIE, X, Y> {
  using t = acap::aie::tile<AIE, X, Y>;
  bool prerun() {
    return true;
  }

  static constexpr int arr_size = 49;
  struct data_type {
    int arr[arr_size];
  };

  void run() {
    assert(false && "test assert");
  }

  void postrun() {
  }
};

int main(int argc, char **argv) {
  acap::aie::device<acap::aie::layout::size<2, 1>> aie;
  // Run up to completion of all the tile programs
  aie.run<prog, tile_memory>();

  return 0;
}
