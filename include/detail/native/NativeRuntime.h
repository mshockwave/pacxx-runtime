//
// Created by mhaidl on 14/06/16.
//

#ifndef PACXX_V2_NATIVERUNTIME_H
#define PACXX_V2_NATIVERUNTIME_H

#include <string>
#include <memory>
#include <map>
#include <list>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include "../IRRuntime.h"
#include "NativeBackend.h"

namespace pacxx
{
  namespace v2
  {
    class NativeRuntime : public IRRuntime<NativeRuntime>
    {
    public:

      using CompilerT = NativeBackend;

      NativeRuntime(unsigned dev_id);
      virtual ~NativeRuntime();

      virtual void link(std::unique_ptr<llvm::Module> M) override;
      virtual Kernel& getKernel(const std::string& name) override;

      virtual size_t getPreferedMemoryAlignment() override;

      template <typename T>
      DeviceBuffer<T>* allocateMemory(size_t count) {
        return nullptr;
      }

      virtual RawDeviceBuffer* allocateRawMemory(size_t bytes) override;
      virtual void deleteRawMemory(RawDeviceBuffer* ptr) override;

    private:
      std::unique_ptr<CompilerT> _compiler;
      std::unique_ptr<llvm::Module> _M;
      llvm::ExecutionEngine* _JITEngine;
    };
  }
}

#endif //PACXX_V2_NATIVERUNTIME_H
