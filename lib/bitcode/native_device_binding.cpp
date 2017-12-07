//===-----------------------------------------------------------*- C++ -*-===//
//
//                       The LLVM-based PACXX Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include </usr/include/math.h> // FIXME: PACXX gets confused with cmath from libc++ on some platforms thats
                               //        why we include math.h with an absolute path here

#include "pacxx/detail/device/DeviceCode.h"
#include "pacxx/detail/device/DeviceFunctionDecls.h"

#ifndef __forceinline__
#define __forceinline__ __attribute__((always_inline))
#endif

extern "C" double rsqrt(double val) {
  return 1.0 / sqrt(val);
}

extern "C" float rsqrtf(float val) {
  return 1.0f / sqrtf(val);
}