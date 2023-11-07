/// This test requires a display
// REQUIRES: aie && false

// RUN: %aie_clang %s -o %t.bin
// RUN: %if_run_on_device %run_on_device %t.bin > %t.check 2>&1
// RUN: %if_run_on_device FileCheck %s --input-file=%t.check

#include "aie.hpp"
#include "ext/graphics.hpp"
#include <complex>

/// The current maximum size of a memory module is 8192 bytes
/// sqrt(8192) ~ 90.5... so 90 is the largest integral value we can put here.
/// at least until the 8192 bytes goes away.
auto constexpr image_size = 90;

template <int X, int Y> struct td_hetero {
  std::uint8_t d[image_size][image_size];
};

int main(int argc, char **argv) {
  aie::device<8, 8> dev;
  aie::queue q(dev);

  aie::ext::graphic_application<uint8_t> a(argc, argv, dev.sizeX, dev.sizeY,
                                           image_size, image_size, 1);
  a.get_image_grid().get_palette().set(aie::ext::palette::rainbow, 100, 2, 0);
  q.submit_hetero<td_hetero>(
      [](auto& ht) {
        ht.single_task([](auto& dt) {
          auto& plane = dt.mem().d;
          // Computation rectangle in the complex plane
          auto constexpr x0 = -2.1, y0 = -1.2, x1 = 0.6, y1 = 1.2;
          auto constexpr D = 100; // Divergence norm
          // Size of an image tile
          auto constexpr xs = (x1 - x0) / dt.size_x() / image_size;
          auto constexpr ys = (y1 - y0) / dt.size_y() / image_size;

          do {
            for (int i = 0; i < image_size; ++i)
              for (int k, j = 0; j < image_size; ++j) {
                std::complex c{x0 + xs * (dt.x() * image_size + i),
                               y0 + ys * (dt.y() * image_size + j)};
                std::complex z{0.0};
                for (k = 0; k <= 255; k++) {
                  z = z * z + c;
                  if (norm(z) > D)
                    break;
                }
                plane[j][i] = k;
              }
          } while (!dt.service().update_image(&plane[0][0], 0, 255));
        });
      },
      aie::add_service(a.service()));
}
