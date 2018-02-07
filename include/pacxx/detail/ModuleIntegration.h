#pragma once
#include <pacxx/detail/common/ExecutorHelper.h>

namespace pacxx {
namespace __integration {
/*FILECONTENT*/

struct __module_registrator {
  __module_registrator(const unsigned char *start, unsigned int len) {
    pacxx::v2::registerModule(reinterpret_cast<const char *>(start),
                              reinterpret_cast<const char *>(start + len));
  }
};

static __module_registrator __reger(kernel, kernel_len);
} // namespace __integration
} // namespace pacxx