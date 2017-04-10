//
// Created by mhaidl on 29/05/16.
//

#ifndef PACXX_V2_EXECUTOR_H
#define PACXX_V2_EXECUTOR_H

#include <cuda.h>

#include "CodePolicy.h"
#include "ModuleLoader.h"
#include "Promise.h"
#include "pacxx/detail/CoreInitializer.h"
#include "pacxx/detail/DeviceBuffer.h"
#include "pacxx/detail/IRRuntime.h"
#include "pacxx/detail/KernelArgument.h"
#include "pacxx/detail/KernelConfiguration.h"
#include "pacxx/detail/MemoryManager.h"
#include "pacxx/detail/common/Exceptions.h"
#include "pacxx/detail/common/Log.h"
#include "pacxx/detail/cuda/CUDARuntime.h"
#include "pacxx/detail/native/NativeRuntime.h"
#include <algorithm>
#include <cstdlib>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Module.h>
#include <memory>
#include <regex>
#include <string>

#ifdef __PACXX_V2_INTEROP
const char *llvm_start = nullptr;
int llvm_size = 0;
const char *reflection_start = nullptr;
int reflection_size = 0;
#else
extern const char llvm_start[];
extern int llvm_size;
extern const char reflection_start[];
extern int reflection_size;
#endif

#ifdef __PACXX_V2_NATIVE_RUNTIME
using Runtime = pacxx::v2::NativeRuntime;
#else
using Runtime = pacxx::v2::CUDARuntime;
#endif

namespace pacxx {
namespace v2 {

class Executor;

Executor &get_executor(unsigned id = 0);
template<typename T = Runtime, typename... Ts> Executor &get_executor(Ts... args);

class Executor {
public:

  static auto& getExecutors(){
    static std::vector<Executor>* executors  = new std::vector<Executor>();
    return *executors; // TODO: free resources at application's exit
  }

  static Executor& get(unsigned id = 0) {
    auto& executors = getExecutors();
    if (executors.empty()) {
      __debug("Default executor crated!");
      Create<Runtime>(0); // TODO: make dynamic fo different devices
    }
    return executors[id];
  }

  template<typename T = Runtime, typename... Ts> static Executor &Create(Ts... args) {
    std::unique_ptr<IRRuntime> rt(new T(args...));
    return Executor::Create(std::move(rt));
  }

  static Executor& Create(std::unique_ptr<IRRuntime> rt, std::string module_bytes = "") {
    auto& executors = getExecutors();
    executors.emplace_back(std::move(rt));
    auto& instance = executors.back();

    instance._id = executors.size() - 1;

    ModuleLoader loader(instance.getLLVMContext());
    if (module_bytes == "") {
      auto M = loader.loadInternal(llvm_start, llvm_size);
      instance.setModule(std::move(M));
      instance.setMSPModule(
          loader.loadInternal(reflection_start, reflection_size));
    } else{
      ModuleLoader loader(instance.getLLVMContext());
      auto M = loader.loadInternal(module_bytes.data(), module_bytes.size());
      instance.setModule(std::move(M));
    }

    return instance;
  }

  Executor(std::unique_ptr<IRRuntime> &&rt) :
      _ctx(new LLVMContext()), _runtime(std::move(rt)),
      _mem_manager(*_runtime) {
    core::CoreInitializer::initialize();
  }

  Executor(Executor&& other) : _mem_manager(std::move(other._mem_manager)){
    _ctx = std::move(other._ctx);
    _runtime = std::move(other._runtime);
    _id = other._id;
    _kernel_translation = std::move(other._kernel_translation);
  }


private:
  std::string cleanName(const std::string &name) {
    auto cleaned_name =
        std::regex_replace(name, std::regex("S[0-9A-Z]{0,9}_"), "");
    cleaned_name =
        std::regex_replace(cleaned_name, std::regex("5pacxx"), ""); // bad hack
    cleaned_name =
        std::regex_replace(cleaned_name, std::regex("2v2"), ""); // bad hack
    // cleaned_name = std::regex_replace(cleaned_name,
    // std::regex("S[0-9A-Z]{0,9}_"), "");
    auto It = cleaned_name.find("$_");
    if (It == std::string::npos)
      return cleaned_name;
    It += 2;
    auto value =
        std::to_string(std::strtol(&cleaned_name[It], nullptr, 10)).size();
    cleaned_name.erase(It + value);
    return cleaned_name;
  }

public:

  unsigned getID() { return _id; };

  void setMSPModule(std::unique_ptr<llvm::Module> M) {
    _runtime->initializeMSP(std::move(M));
  }

  void setModule(std::unique_ptr<llvm::Module> M);

  void setModule(std::string module_bytes);

  template<typename L, typename... Args>
  void run(const L &lambda, KernelConfiguration config, Args &&... args) {
    // auto& dev_lambda = _mem_manager.getTemporaryLambda(lambda);
    auto &K = get_kernel_by_name(typeid(L).name(), config,
                                 std::forward<const L>(lambda),
                                 std::forward<Args>(args)...);
    K.launch();
  }

  template<typename L, typename CallbackFunc, typename... Args>
  void run_with_callback(const L &lambda, KernelConfiguration config,
                         CallbackFunc &&cb, Args &&... args) {
    auto &K = get_kernel_by_name(typeid(L).name(), config,
                                 std::forward<const L>(lambda),
                                 std::forward<Args>(args)...);
    K.setCallback(std::move(cb));
    K.launch();
  }

  template<typename... Args>
  auto &get_kernel_by_name(std::string name, KernelConfiguration config,
                           Args &&... args) {

    std::string FName;
    const llvm::Module &M = _runtime->getModule();
    auto it = _kernel_translation.find(name);
    if (it == _kernel_translation.end()) {
      auto clean_name = cleanName(name);
      for (auto &p : _kernel_translation)
        if (p.first.find(clean_name) != std::string::npos) {
          FName = p.second;
          //_kernel_translation[name] = F.getName().str();
        }
    } else
      FName = it->second;

    auto F = M.getFunction(FName);
    if (!F) {
      throw common::generic_exception("Kernel function not found in module! " +
          cleanName(name));
    }

    auto &K = _runtime->getKernel(FName);
    K.setName(FName);
    K.configurate(config);

    size_t buffer_size = 0;
    __verbose("Executor arg size ", F->arg_size());
    std::vector<size_t> arg_offsets(F->arg_size());

    int offset = 0;

    std::transform(F->arg_begin(), F->arg_end(), arg_offsets.begin(),
                   [&](const auto &arg) {
                     auto arg_size =
                         M.getDataLayout().getTypeAllocSize(arg.getType());
                     auto arg_alignment =
                         M.getDataLayout().getPrefTypeAlignment(arg.getType());

                     auto arg_offset =
                         (offset + arg_alignment - 1) & ~(arg_alignment - 1);
                     offset = arg_offset + arg_size;
                     buffer_size = offset;
                     return arg_offset;
                   });

    std::vector<char> args_buffer(buffer_size);
    __verbose("host arg buffer size is: ", K.getHostArgumentsSize());
    std::vector<char> host_args_buffer(K.getHostArgumentsSize());

    auto ptr = args_buffer.data();
    auto hptr = host_args_buffer.data();
    size_t i = 0;
    ptrdiff_t hoffset = 0;
    void *lambdaPtr = nullptr;

    common::for_first_in_arg_pack(
        [&](auto &lambda) { lambdaPtr = (void *) &lambda; },
        std::forward<Args>(args)...);

    common::for_each_in_arg_pack(
        [&](auto &&arg) {
          if (i == 0) {
            if (host_args_buffer.size() > 0)
              std::memcpy(hptr + hoffset, &lambdaPtr,
                          sizeof(decltype(lambdaPtr)));
            hoffset += sizeof(decltype(lambdaPtr));
          } else if (host_args_buffer.size() > 0) {
            std::memcpy(hptr + hoffset, &arg, sizeof(decltype(arg)));
            hoffset += sizeof(decltype(arg));
          }

          auto offset = arg_offsets[i++];
          auto targ = meta::memory_translation{}(_mem_manager, arg);
          //          __warning(sizeof(decltype(arg)), " ", hoffset, " ",
          //          typeid(arg).name());
          std::memcpy(ptr + offset, &targ, sizeof(decltype(targ)));
        },
        std::forward<Args>(args)...);

    K.setHostArguments(host_args_buffer);
    K.setArguments(args_buffer);

    _runtime->evaluateStagedFunctions(K);

    return K;
  }

  template<typename... Args>
  void run_interop(std::string name, KernelConfiguration config,
                   const std::vector<KernelArgument> &args) {

    const llvm::Module &M = _runtime->getModule();
    const llvm::Function *F = M.getFunction(name);

    if (!F)
      throw common::generic_exception("Kernel function not found in module! " +
          name);

    size_t buffer_size = 0;
    std::vector<size_t> arg_offsets(F->arg_size());

    int offset = 0;

    std::transform(F->arg_begin(), F->arg_end(), arg_offsets.begin(),
                   [&](const auto &arg) {
                     auto arg_size =
                         M.getDataLayout().getTypeAllocSize(arg.getType());
                     auto arg_alignment =
                         M.getDataLayout().getPrefTypeAlignment(arg.getType());

                     auto arg_offset =
                         (offset + arg_alignment - 1) & ~(arg_alignment - 1);
                     offset = arg_offset + arg_size;
                     buffer_size = offset;
                     return arg_offset;
                   });

    std::vector<char> args_buffer(buffer_size);
    auto ptr = args_buffer.data();
    size_t i = 0;

    for (const auto &arg : args) {
      auto offset = arg_offsets[i++];
      std::memcpy(ptr + offset, arg.address, arg.size);
    }

    auto &K = _runtime->getKernel(F->getName().str());
    K.configurate(config);
    K.setArguments(args_buffer);
    K.launch();
  }

  template<typename T>
  DeviceBuffer<T> &allocate(size_t count, T *host_ptr = nullptr) {
    __verbose("allocating memory: ", sizeof(T) * count);

    switch(_runtime->getRuntimeType())
    {
    case RuntimeType::CUDARuntimeTy:
      return *static_cast<CUDARuntime&>(*_runtime).template allocateMemory(count, host_ptr);
    case RuntimeType::NativeRuntimeTy:
      return *static_cast<NativeRuntime&>(*_runtime).template allocateMemory(count, host_ptr);
    }

    throw pacxx::common::generic_exception("unreachable code");
  }

  RawDeviceBuffer &allocateRaw(size_t bytes) {
    __verbose("allocating raw memory: ", bytes);
    return *_runtime->allocateRawMemory(bytes);
  }

  template<typename T> void free(DeviceBuffer<T> &buffer) {
    switch(_runtime->getRuntimeType())
    {
    case RuntimeType::CUDARuntimeTy:
      return *static_cast<CUDARuntime&>(*_runtime).template deleteMemory(buffer);
    case RuntimeType::NativeRuntimeTy:
      return *static_cast<NativeRuntime&>(*_runtime).template deleteMemory(buffer);
    }
  }

  void freeRaw(RawDeviceBuffer &buffer) { _runtime->deleteRawMemory(&buffer); }

  auto &mm() { return _mem_manager; }

  auto &rt() { return *_runtime; }

  void synchronize() { _runtime->synchronize(); }

  auto &getPassManager() { return _runtime->getPassManager(); }

  template<typename PromisedTy, typename... Ts>
  auto &getPromise(Ts &&... args) {
    auto promise = new BindingPromise<PromisedTy>(std::forward<Ts>(args)...);
    return *promise;
  };

  template<typename PromisedTy>
  void forgetPromise(BindingPromise<PromisedTy> &instance) {

    delete &instance;
  };

  LLVMContext &getLLVMContext() { return *_ctx; }

private:
  std::unique_ptr<LLVMContext> _ctx;
  std::unique_ptr<IRRuntime> _runtime;
  MemoryManager _mem_manager;
  std::map<std::string, std::string> _kernel_translation;
  unsigned _id;
};
}
}

#endif // PACXX_V2_EXECUTOR_H