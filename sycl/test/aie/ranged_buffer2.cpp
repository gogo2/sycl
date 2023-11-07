// REQUIRES: aie

// RUN: %aie_clang %s -o %t.bin
// RUN: %if_run_on_device %run_on_device %t.bin > %t.check 2>&1
// RUN: %if_run_on_device FileCheck %s --input-file=%t.check

#include "aie.hpp"
#include <numeric>

int main() {
  constexpr std::size_t size = 10;
  aie::device<1, 1> dev;
  // Create a buffer of 10 elements initialized to 0
  aie::buffer<int> buff(10);
  std::iota(buff.begin(), buff.begin() + 5, 0);
  for (int i = 0; i < size; i++) {
    std::cout << "buff[" << i << "]=" << buff[i] << std::endl;
  }
  aie::queue q(dev);
  q.submit_uniform([&](auto& ht) {
    /// only access the last five elements of the buffer in read and write.
    aie::accessor acc = aie::buffer_range(ht, buff)
                            .read_range(5, buff.size())
                            .write_range(5, buff.size());
    ht.single_task([=](auto& dt) {
      /// The accessor on device is only on the last 5 elements
      for (int i = 0; i < acc.size(); i++) {
        acc[i] = i + 5;
      }
    });
  });
  for (int i = 0; i < size; i++) {
    std::cout << "buff[" << i << "]=" << buff[i] << std::endl;
    assert(buff[i] == i);
  }
}
// CHECK: exit_code=0
