//
// Created by mhaidl on 30/05/16.
//
#include <cuda.h>
#include <cuda_runtime.h>
#include "detail/cuda/CUDAErrorDetection.h"
#include "detail/common/Common.h"
#include "detail/common/Exceptions.h"

namespace{
  static std::string _cudaGetErrorEnum(CUresult error) {
    const char *p = nullptr;
    cuGetErrorString(error, &p);
    return std::string(p);
  }
}


namespace pacxx
{

  namespace v2
  {
    bool checkCUDACall(CUresult result, char const *const func,
                   const char *const file, int const line) {
      if (result != cudaError_enum::CUDA_SUCCESS) {
        throw common::generic_exception(
            common::to_string("CUDA (driver api) error: ", func, " failed! ",
                              _cudaGetErrorEnum(result), " (", result, ") ",
                              common::get_file_from_filepath(file), ":", line));
      }

      return true;
    }

    bool checkCUDACall(cudaError_t result, char const *const func,
                   const char *const file, int const line) {
      if (result != cudaError::cudaSuccess) {
        throw common::generic_exception(
            common::to_string("CUDA (runtime api) error: ", func, " failed! ",
                              cudaGetErrorString(result), " (", result, ") ",
                              common::get_file_from_filepath(file), ":", line));
      }

      return true;
    }

  }
}