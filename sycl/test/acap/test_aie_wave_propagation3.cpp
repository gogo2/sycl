// REQUIRES: acap

// RUN: %acap_clang %s -o %s.bin | FileCheck %s -check-prefix CHECK-MERGING
// RUN: %add_acap_result %s.bin
// RUN: rm %s.bin

// check that we only compile 16 device kernel via chess
// CHECK-MERGING-COUNT-16: Linking Kernel
// CHECK-MERGING-NOT: Linking Kernel

/* Demo of wave propagation for AI Engine

   Simulation with a conic drop, a circle shoal and a square harbor.

   Recycle MINES ParisTech/ISIA/Telecom Bretagne MSc hands-on HPC labs
   from Ronan Keryell

   https://en.wikipedia.org/wiki/Boussinesq_approximation_(water_waves)
   Joseph Valentin Boussinesq, 1872

   
*/

/** Predicate for time-step comparison with sequential cosimulation

    0: for no co-simulation

    1: compare the parallel execution with sequential execution
*/
#define COMPARE_WITH_SEQUENTIAL_EXECUTION 0

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <iostream>

#include <experimental/mdspan>

#include "triSYCL/vendor/Xilinx/graphics.hpp"
#include <sycl/sycl.hpp>

// Some headers used when debugging
#include <chrono>
#include <thread>
using namespace std::chrono_literals;

#include <boost/thread.hpp>

using namespace sycl::vendor::xilinx;

// The size of the machine to use
// using layout = acap::aie::layout::size<5,4>;
// For a 1920x1080 display
// using layout = acap::aie::layout::size<18,8>;
using layout = acap::aie::layout::size<8, 8>;
// For a 3440x1440 display
// using layout = acap::aie::layout::size<33,12>;
using geography = acap::aie::geography<layout>;
boost::barrier cpu_barrier{geography::size};

using data_type = double;

auto constexpr K = 1.0 / 300.0;
auto constexpr g = 9.81;
auto constexpr alpha = K * g;
auto constexpr damping = 0.999;

auto constexpr image_size = 20;
auto constexpr zoom = 5;
/// Add a drop almost between tile (1,1) and (2,2)
auto constexpr x_drop = image_size * 1 - image_size / 2 - 1;
auto constexpr y_drop = image_size * 1 - image_size / 2 - 1;
auto constexpr drop_value = 100.0;
auto constexpr drop_radius = 5.0;

/** Time-step interval between each display.
    Use 1 to display all the frames, 2 for half the frame and so on. */
auto constexpr display_time_step = 1;

graphics::application<data_type> a;

auto epsilon = 0.01;

#if COMPARE_WITH_SEQUENTIAL_EXECUTION == 1
/** Compare the values of 2 2D mdspan of the same geometry

    Display any discrepancy between an acap and reference mdspan
*/
auto compare_2D_mdspan = [](auto message, const auto &acap, const auto &ref) {
  assert(acap.extent(0) == ref.extent(0));
  assert(acap.extent(1) == ref.extent(1));
  for (int j = 0; j < acap.extent(0); ++j)
    for (int i = 0; i < acap.extent(1); ++i)
      if (std::abs(acap(j, i) - ref(j, i)) > epsilon) {
        TRISYCL_DUMP_T(std::dec << '\t' << message << " acap(" << j << ',' << i
                                << ") = " << acap(j, i) << "  ref(" << j << ','
                                << i << ") = " << ref(j, i));
      }
};
#endif

/// Compute the square power of a value
constexpr auto square = [](auto v) constexpr { return v * v; };

/// Compute the contribution of a drop to the water height
constexpr auto add_a_drop = [](auto x, auto y) constexpr {
  // The square radius to the drop center
  auto r = square(x - x_drop) + square(y - y_drop);
  // A cone of height drop_value centered on the drop center
  return r < square(drop_radius)
             ? drop_value * (square(drop_radius) - r) / square(drop_radius)
             : 0;
};

/// Add a circular shoal in the water with half the depth
constexpr auto shoal_factor = [] (auto x, auto y) constexpr {
  /// The shoal center coordinates
  auto constexpr x_shoal = image_size*8 - 3;
  auto constexpr y_shoal = image_size*4;
  auto constexpr shoal_radius = 200.0;

  // The square radius to the shoal center
  auto r = square(x - x_shoal) + square(y - y_shoal);
  // A disk centered on the shoal center
  return r < square(shoal_radius) ? 0.5 : 1;
};

/// Add a square harbor in the water
constexpr auto is_harbor = [](auto x, auto y) constexpr -> bool {
  /// The square harbor center coordinates
  auto constexpr x_harbor = image_size * 3 - image_size / 3;
  auto constexpr y_harbor = image_size * 2 - image_size / 3;
  auto constexpr length_harbor = image_size;

  // A square centered on the harbor center
  auto harbor =
      x_harbor - length_harbor / 2 <= x && x <= x_harbor + length_harbor / 2 &&
      y_harbor - length_harbor / 2 <= y && y <= y_harbor + length_harbor / 2;
  // Add also a breakwater below
  auto constexpr width_breakwater = image_size / 3;
  auto breakwater = x_harbor <= x && x <= x_harbor + width_breakwater &&
                    y < y_harbor - image_size
                    // Add some 4-pixel holes every image_size/2
                    && (y / 4) % (image_size / 8);
  return false;
  // return harbor || breakwater;
};

/// A sequential reference implementation of wave propagation
template <auto size_x, auto size_y, auto display_tile_size>
struct reference_wave_propagation {
  // std::experimental::extents<Geography::x_size,
  //                                                      Geography::y_size>
  using space = std::experimental::mdspan<double, std::experimental::extents<size_y, size_x>>;
  // It would be nice to have a constexpr static member to express this,
  // but right now size() is a member function
  double u[size_x][size_y];
  double v[size_x][size_y];
  double w[size_x][size_y];
  double side[size_x][size_y];
  double depth[size_x][size_y];

  /// Initialize the state variables
  reference_wave_propagation() {
    for (int j = 0; j < size_y; ++j)
      for (int i = 0; i < size_x; ++i) {
        // No u[j][i] syntax too like in Boost.Multi_Array ?
        u[j][i] = v[j][i] = 0;
        // side(j,i) = K*(!is_harbor(i, j));
        side[j][i] = K;
        // depth(j,i) = 2600.0*shoal_factor(i, j);
        depth[j][i] = 0;
        w[j][i] = add_a_drop(i, j);
      }
  }

  /// Compute a time-step of wave propagation
  void compute() {
    for (int j = 0; j < size_y; ++j)
      for (int i = 0; i < size_x - 1; ++i) {
        // dw/dx
        auto north = w[j][i + 1] - w[j][i];
        // Integrate horizontal speed
        u[j][i] += north * alpha;
      }
    for (int j = 0; j < size_y - 1; ++j)
      for (int i = 0; i < size_x; ++i) {
        // dw/dy
        auto vp = w(j + 1, i) - w[j][i];
        // Integrate vertical speed
        v[j][i] += vp * alpha;
      }
    for (int j = 1; j < size_y; ++j)
      for (int i = 1; i < size_x; ++i) {
        // div speed
        auto wp = (u[j][i] - u(j, i - 1)) + (v[j][i] - v(j - 1, i));
        wp *= side[j][i] * (depth[j][i] + w[j][i]);
        // Integrate depth
        w[j][i] += wp;
        // Add some dissipation for the damping
        w[j][i] *= damping;
      }
  }

  void validate() {
    for (int j = 0; j < size_y / display_tile_size; ++j)
      for (int i = 0; i < size_x / display_tile_size; ++i) {
        /* Split the data in sub-windows with a subspan

           Display actually one redundant line/column on each
           South/West to mimic the halo in the ACAP case
        */
        auto sp = std::experimental::submdspan(
            w,
            std::make_pair(j * display_tile_size, (j + 1) * display_tile_size),
            std::make_pair(i * display_tile_size, (i + 1) * display_tile_size));
        a.validate_tile_data_image(i, j, sp, -1.0, 1.0);
      }
  }

  /// Run the wave propagation
  void run() {
    /// Loop on simulated time
    validate();
    while (!a.is_done()) {
      compute();
      validate();
    }
  }

  template <typename Array, typename MDspan_ref>
  void compare_with_sequential_reference_e(const char *message, int x, int y,
                                           Array &arr, const MDspan_ref &ref) {
    const std::experimental::mdspan<
        double, std::experimental::extents<image_size, image_size>>
        md{&arr[0][0]};

    // Take into account 1 line/column of overlapping halo
    int x_offset = md.extent(1) - 1;
    int y_offset = md.extent(0) - 1;
    auto mdref = std::experimental::submdspan(
        ref, std::make_pair(y * y_offset, 1 + (y + 1) * y_offset),
        std::make_pair(x * x_offset, 1 + (x + 1) * x_offset));
    compare_2D_mdspan(message, md, mdref);
  }

  /* The global time of the simulation

     Do not put it inside compare_with_sequential_reference because,
     since it is templated, there is then an instance per tile and the
     chaos happens
  */
  static inline int global_time = 0;
  static inline std::mutex protect_time;

  template <typename Mem>
  void compare_with_sequential_reference(int time, int x, int y, Mem &m) {

#if COMPARE_WITH_SEQUENTIAL_EXECUTION
    {
      std::lock_guard lg{protect_time};
      TRISYCL_DUMP_T(std::dec << "TILE(" << x << ',' << y << ") Time local: "
                              << time << ", global: " << global_time);
      if (global_time != time) {
        /* Advance the sequential computation by one step so that we
           can do the comparison */
        compute();
        ++global_time;
      }
      compare_with_sequential_reference_e("w", x, y, m.w, w);
      compare_with_sequential_reference_e("u", x, y, m.u, u);
      compare_with_sequential_reference_e("v", x, y, m.v, v);
    }
#endif
  }
};

/** A sequential reference implementation of the wave propagation

    Use (image_size - 1) for the tile size to skip the halo zone of 1
    pixel in X and Y
*/
reference_wave_propagation<
    (image_size - 1) * acap::aie::geography<layout>::x_size + 1,
    (image_size - 1) * acap::aie::geography<layout>::y_size + 1, image_size - 1>
    seq;

/// All the memory modules are the same
template <typename AIE, int X, int Y>
struct memory : acap::aie::memory<AIE, X, Y> {
  data_type u[image_size][image_size];     //< Horizontal speed
  data_type v[image_size][image_size];     //< Vertical speed
  data_type w[image_size][image_size];     //< Local delta depth
  data_type side[image_size][image_size];  //< Hard wall limit
  data_type depth[image_size][image_size]; //< Average depth
};

/// All the tiles run the same program
template <typename AIE, int X, int Y> struct tile : acap::aie::tile<AIE, X, Y> {
  using t = acap::aie::tile<AIE, X, Y>;

  void initialize_space() {
    auto &m = t::mem();
    for (int j = 0; j < image_size; ++j)
      for (int i = 0; i < image_size; ++i) {
        m.u[j][i] = m.v[j][i] = 0;
        // m.side[j][i] = K * (!is_harbor(i + (image_size - 1) * X,
        //                                j + (image_size - 1) * Y));
        m.side[j][i] = K;
        m.depth[j][i] = 1.0;
        // m.depth[j][i] = 2600.0 * shoal_factor(i + (image_size - 1) * X,
        //                                       j + (image_size - 1) * Y);
        // Add a drop using the global coordinate taking into account the halo
        m.w[j][i] =
            add_a_drop(i + (image_size - 1) * X, j + (image_size - 1) * Y);
        // m.w[j][i] = add_a_drop(i, j);
      }
  }

  void compute() {
    auto &m = t::mem();

    for (int j = 0; j < image_size; ++j)
      for (int i = 0; i < image_size - 1; ++i) {
        // dw/dx
        auto north = m.w[j][i + 1] - m.w[j][i];
        // Integrate horizontal speed
        m.u[j][i] += north * alpha;
      }

    for (int j = 0; j < image_size - 1; ++j)
      for (int i = 0; i < image_size; ++i) {
        // dw/dy
        auto vp = m.w[j + 1][i] - m.w[j][i];
        // Integrate vertical speed
        m.v[j][i] += vp * alpha;
      }

#ifdef __SYCL_XILINX_AIE__
    t::barrier();
#else
    cpu_barrier.wait();
#endif

    // Transfer first column of u to next memory module to the West
    if constexpr (Y & 1) {
      if constexpr (t::is_memory_module_east()) {
        auto &east = t::mem_east();
        for (int j = 0; j < image_size; ++j)
          m.u[j][image_size - 1] = east.u[j][0];
      }
    }
    if constexpr (!(Y & 1)) {
      if constexpr (t::is_memory_module_west()) {
        auto &west = t::mem_west();
        for (int j = 0; j < image_size; ++j)
          west.u[j][image_size - 1] = m.u[j][0];
      }
    }

    if constexpr (t::is_memory_module_south()) {
      auto &below = t::mem_south();
      for (int i = 0; i < image_size; ++i)
        below.v[image_size - 1][i] = m.v[0][i];
    }

#ifdef __SYCL_XILINX_AIE__
    t::barrier();
#else
    cpu_barrier.wait();
#endif

    for (int j = 1; j < image_size; ++j)
      for (int i = 1; i < image_size; ++i) {
        // div speed
        auto wp = (m.u[j][i] - m.u[j][i - 1]) + (m.v[j][i] - m.v[j - 1][i]);
        wp *= m.side[j][i] * (m.depth[j][i] + m.w[j][i]);
        // Integrate depth
        m.w[j][i] += wp;
        // Add some dissipation for the damping
        m.w[j][i] *= damping;
      }

#ifdef __SYCL_XILINX_AIE__
    t::barrier();
#else
    cpu_barrier.wait();
#endif

    if constexpr (t::is_memory_module_north()) {
      auto &above = t::mem_north();
      for (int i = 0; i < image_size; ++i)
        above.w[0][i] = m.w[image_size - 1][i];
    }

#ifdef __SYCL_XILINX_AIE__
    t::barrier();
#else
    cpu_barrier.wait();
#endif

    // Transfer last line of w to next memory module on the East
    if constexpr (Y & 1) {
      if constexpr (t::is_memory_module_east()) {
        auto &east = t::mem_east();
        for (int j = 0; j < image_size; ++j)
          east.w[j][0] = m.w[j][image_size - 1];
      }
    }
    if constexpr (!(Y & 1)) {
      if constexpr (t::is_memory_module_west()) {
        auto &west = t::mem_west();
        for (int j = 0; j < image_size; ++j)
          m.w[j][0] = west.w[j][image_size - 1];
      }
    }

#ifdef __SYCL_XILINX_AIE__
    t::barrier();
#else
    cpu_barrier.wait();
#endif
  }
  // void display_stop() {
  //   while (!a.is_done()) {
  //     a.update_tile_data_image(t::x, t::y, &t::mem().w[0][0], -1.0, 1.0);
  //   }
  // }

  void full_display(const char *str) {
#if defined(__SYCL_DEVICE_ONLY__)
    auto &m = t::mem();
    t::log(str);
    t::log("u v w side depth\n");
    t::log((uint32_t)&m.u[0][0]);
    a.update_tile_data_image(t::x, t::y, &m.u[0][0], -1.0, 1.0);
#else
#endif
  }

  // memory<AIE, X, Y> m;

  // data_type frame[image_size][image_size];
  void display() {
    auto &m = t::mem();
    // __builtin_memcpy(frame, &m.w[0][0], image_size * image_size *
    // sizeof(data_type)); a.update_tile_data_image(t::x, t::y, &frame[0][0],
    // -1.0, 1.0);
    a.update_tile_data_image(t::x, t::y, &m.w[0][0], -1.0, 1.0);
  }

  void run() {
    initialize_space();
    display();
    // full_display("initial state\n");
    // display_stop();
    // std::experimental::mdspan<data_type, image_size, image_size> md {
    // &m.w[0][0] };
    /// Loop on simulated time
    for (int time = 0; !a.is_done(); ++time) {
      compute();
      // full_display("finished frame");
      // Display every display_time_step
      // if (time % display_time_step == 0) {
      display();
      // }
      // a.update_tile_data_image(t::x, t::y, md, -1.0, 1.0);
      // for (int i = 0; i < 10000000; i++)
      //   acap_intr::memory_fence();
    }
  }

  // void postrun() {
  //   data_type dev_out[image_size][image_size];
  //   t::get_dev_handle().memcpy_d2h(&dev_out, acap::hw_mem::args_beg_off +
  //   sizeof(dev_out) * 2,
  //                                  sizeof(dev_out));
  //   run();
  //   data_type avg_delta = 0;
  //   for (int j = 0; j < image_size; j++)
  //     for (int i = 0; i < image_size; i++) {
  //       bool is_ok = false;
  //       if constexpr (std::is_floating_point_v<data_type>) {
  //         constexpr data_type epsilon = 0.01;
  //         avg_delta = (avg_delta * i + std::abs(dev_out[i][j] - m.w[i][j])) /
  //         (i + 1); is_ok = (std::abs(dev_out[i][j] - m.w[i][j]) < epsilon);
  //       } else
  //         is_ok = (dev_out[i][j] == m.w[i][j]);
  //     }
  //   std::cout << "avg_delta = " << std::fixed << std::showpoint
  //             << std::setprecision(10) << avg_delta << "\n";
  // }
};

int main(int argc, char *argv[]) {
  // An ACAP version of the wave propagation
  acap::aie::device<layout> d;

  a.set_device(d);
#ifdef __SYCL_XILINX_AIE__
  // a.enable_data_validation();
#endif
  a.start(argc, argv, decltype(d)::geo::x_size, decltype(d)::geo::y_size,
          image_size, image_size, zoom);
  // Clip the level 127 which is the 0 level of the simulation
  a.image_grid().get_palette().set(graphics::palette::rainbow, 150, 2, 127);

#ifdef __SYCL_XILINX_AIE__
  // a.start_data_validation([&]() { seq.run(); });
#endif
  // Launch the AI Engine program
  d.run<tile, memory>();
  // Wait for the graphics to stop
  // a.wait();
}
