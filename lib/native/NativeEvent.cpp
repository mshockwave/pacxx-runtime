//
// Created by m_haid02 on 29.06.17.
//

#include <chrono>
#include "pacxx/detail/native/NativeEvent.h"

namespace pacxx{
namespace v2{

void NativeEvent::start() {
  _start = std::chrono::high_resolution_clock::now();
}

void NativeEvent::stop() {
  _end = std::chrono::high_resolution_clock::now();
}

double NativeEvent::result() {
  return std::chrono::duration_cast<std::chrono::microseconds>(_end - _start).count();
}

}
}