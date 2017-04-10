//
// Created by mhaidl on 14/06/16.
//

#ifndef PACXX_V2_NATIVERUNTIME_H
#define PACXX_V2_NATIVERUNTIME_H

#include "../IRRuntime.h"
#include "NativeBackend.h"
#include "NativeDeviceBuffer.h"
#include "NativeKernel.h"
#include "pacxx/detail/common/Exceptions.h"
#include "pacxx/detail/msp/MSPEngine.h"
#include <list>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <map>
#include <memory>
#include <string>
#include <tbb/task_scheduler_init.h>
#include <thread>

namespace pacxx {
namespace v2 {

class NativeRuntime : public IRRuntime {
public:
  using CompilerT = NativeBackend;

  NativeRuntime(unsigned dev_id);
  virtual ~NativeRuntime();

  virtual RuntimeType getRuntimeType() override;

  virtual void link(std::unique_ptr<llvm::Module> M) override;

  virtual Kernel &getKernel(const std::string &name) override;

  virtual size_t getPreferedMemoryAlignment() override;

  template <typename T>
  DeviceBuffer<T> *allocateMemory(size_t count, T *host_ptr) {
    NativeRawDeviceBuffer rawBuffer;
    if (host_ptr)
      rawBuffer.allocate(count * sizeof(T), reinterpret_cast<char *>(host_ptr));
    else
      rawBuffer.allocate(count * sizeof(T));

    auto wrapped = new NativeDeviceBuffer<T>(std::move(rawBuffer));
    _memory.push_back(std::unique_ptr<DeviceBufferBase>(
        static_cast<DeviceBufferBase *>(wrapped)));
    return wrapped;
  }

  template <typename T> DeviceBuffer<T> *translateMemory(T *ptr) {
    auto It =
        std::find_if(_memory.begin(), _memory.end(), [&](const auto &element) {
          return reinterpret_cast<NativeDeviceBuffer<T> *>(element.get())
                     ->get() == ptr;
        });

    if (It != _memory.end())
      return reinterpret_cast<DeviceBuffer<T> *>(It->get());
    else
      throw common::generic_exception(
          "supplied pointer not found in translation list");
  }

  template <typename T> void deleteMemory(DeviceBuffer<T> *ptr) {
    auto It =
        std::find_if(_memory.begin(), _memory.end(),
                     [&](const auto &element) { return element.get() == ptr; });

    if (It != _memory.end())
      _memory.erase(It);
  }

  virtual RawDeviceBuffer *allocateRawMemory(size_t bytes) override;

  virtual void deleteRawMemory(RawDeviceBuffer *ptr) override;

  virtual void initializeMSP(std::unique_ptr<llvm::Module> M) override;

  virtual void evaluateStagedFunctions(Kernel &K) override;

  virtual void requestIRTransformation(Kernel &K) override;

  virtual const llvm::Module &getModule() override;

  virtual void synchronize() override;

  virtual llvm::legacy::PassManager &getPassManager() override;

private:
  void compileAndLink();

private:
  llvm::Module *_CPUMod;
  std::unique_ptr<llvm::Module> _M, _rawM;
  std::unique_ptr<CompilerT> _compiler;
  std::map<std::string, std::unique_ptr<NativeKernel>> _kernels;
  std::list<std::unique_ptr<DeviceBufferBase>> _memory;
  bool _delayed_compilation;
  v2::MSPEngine _msp_engine;
};
}
}

#endif // PACXX_V2_NATIVERUNTIME_H