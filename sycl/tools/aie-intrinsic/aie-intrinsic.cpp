//==-- aie-intrinsic.cpp - collection of intrisics for the aie target ----==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#define __AIE_RT__
#include "aie-intrinsic.h"

namespace aie::intrinsics {

// All the functions called here are just intrinsics. They do not have symbols but are
// just a function-like representation of an AIE instruction, so they should not be
// able to collide with another symbol

int get_coreid() { return ::get_coreid(); }

void memory_fence() { ::chess_memory_fence();  }
void separator_scheduler() { ::chess_separator_scheduler(); }

void acquire(unsigned id, unsigned val) { return ::acquire(id, val); }
void release(unsigned id, unsigned val) { return ::release(id, val); }
void acquire(unsigned id) { return ::acquire(id); }
void release(unsigned id) { return ::release(id); }

void core_done() { ::done(); }

extern "C" void _Z13finish_kernelv();

void soft_done() {
  _Z13finish_kernelv();
}

void nop5() { ::nop(5); }

uint32_t sread(int stream_idx) { return ::get_ss(stream_idx); }
void swrite(int stream_idx, uint32_t val, int tlast) { return ::put_ms(stream_idx, val, tlast); }

/// CHESS stream intrinsics use types like v4int32 or v8acc48 that do not exist
/// in our SYCL compiler, so all data to be read or written to a stream is
/// passed by a pointer to its start. We do not copy any of this data just load
/// it and write it to a stream or read for a stream and write it to memory
void stream_read4(char* out_buffer, int stream_idx) {
  *reinterpret_cast<uint32_t*>(out_buffer) = ::get_ss(stream_idx);
}
void stream_write4(const char* in_buffer, int stream_idx, int tlast) {
  ::put_ms(stream_idx, *reinterpret_cast<const uint32_t*>(in_buffer), tlast);
}
void stream_read16(char* out_buffer, int stream_idx) {
  *reinterpret_cast<v4int32*>(out_buffer) = ::getl_wss(stream_idx);
}
void stream_write16(const char* in_buffer, int stream_idx, int tlast) {
  ::put_wms(stream_idx, *reinterpret_cast<const v4int32*>(in_buffer), tlast);
}

void cstream_read48(char* out_buffer) {
  *reinterpret_cast<v8acc48*>(out_buffer) = ::get_scd();
}
void cstream_write48(const char* in_buffer) {
  ::put_mcd(*reinterpret_cast<const v8acc48*>(in_buffer));
}

}
