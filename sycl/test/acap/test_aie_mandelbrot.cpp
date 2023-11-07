// REQUIRES: acap

// RUN: %acap_clang %s -o %s.bin | FileCheck %s -check-prefix CHECK-MERGING
// RUN: %add_acap_result %s.bin
// RUN: rm %s.bin

// check that we only compile 2 device kernel via chess
// CHECK-MERGING-COUNT-2: Linking Kernel
// CHECK-MERGING-NOT: Linking Kernel

#include <complex>
#include <cstdint>
#include <iostream>

#include <sycl/sycl.hpp>
#include <sycl/vendor/Xilinx/graphics.hpp>

using namespace sycl::vendor::xilinx;

/// The current maximum size of a memory module is 8192 bytes
/// sqrt(8192) ~ 90.5... so 90 is the largest integral value we can put here. at
/// least until the 8192 bytes goes away.
auto constexpr image_size = 90;
graphics::application<uint8_t> a;

// All the memory modules are the same
template <typename AIE, int X, int Y>
struct memory : acap::aie::memory<AIE, X, Y> {
  // The local pixel tile inside the complex plane
  std::uint8_t plane[image_size][image_size];
};

// All the tiles run the same Mandelbrot program
template <typename AIE, int X, int Y>
struct mandelbrot : acap::aie::tile<AIE, X, Y> {
  using t = acap::aie::tile<AIE, X, Y>;
  // Computation rectangle in the complex plane
  static auto constexpr x0 = -2.1, y0 = -1.2, x1 = 0.6, y1 = 1.2;
  static auto constexpr D = 100; // Divergence norm
  // Size of an image tile
  static auto constexpr xs = (x1 - x0)/t::geo::x_size/image_size;
  static auto constexpr ys = (y1 - y0)/t::geo::y_size/image_size;

  void run() {
    // Access to its own memory
    auto &m = t::mem();
    while (!a.is_done()) {
      for (int i = 0; i < image_size; ++i)
        for (int k, j = 0; j < image_size; ++j) {
          std::complex c{x0 + xs * (t::x_coord() * image_size + i),
                         y0 + ys * (t::y_coord() * image_size + j)};
          std::complex z{0.0};
          for (k = 0; k <= 255; k++) {
            z = z * z + c;
            if (norm(z) > D)
              break;
          }
          m.plane[j][i] = k;
        }
      a.update_tile_data_image(t::x_coord(), t::y_coord(), &m.plane[0][0], 0,
                               255);
    }
  }
  void postrun() {}
  void prerun() {}
};

int main(int argc, char *argv[]) {
  acap::aie::device<acap::aie::layout::size<50, 8>> aie;
  // Open a graphic view of a ME array
  a.set_device(aie);
  a.start(argc, argv, decltype(aie)::geo::x_size,
          decltype(aie)::geo::y_size,
          image_size, image_size, 1);
  a.image_grid().get_palette().set(graphics::palette::rainbow, 100, 2, 0);

  // Launch the AI Engine program
  aie.run<mandelbrot, memory>();
  // Wait for the graphics to stop
  a.wait();
}
