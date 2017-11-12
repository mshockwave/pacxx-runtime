//===-----------------------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "pacxx/detail/common/transforms/ModuleHelper.h"

#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

using namespace llvm;
using namespace std;
using namespace pacxx;

namespace {
struct PACXXCodeGenPrepare : public ModulePass {
  static char ID;
  PACXXCodeGenPrepare() : ModulePass(ID) {}
  virtual ~PACXXCodeGenPrepare() {}

  virtual bool runOnModule(Module &M) {

    vector<pair<Instruction *, Instruction *>> to_insert;
    auto visitor = make_CallVisitor([&](CallInst *I) {
      if (!I)
        return;

      if (!I->isInlineAsm()) {

        if (!isa<Function>(I->getCalledValue())) {
          return;
        }

        auto F = I->getCalledFunction();

        if (!F)
          return;

        // mark all called functions as always inline to pull them into the kernel
        if (F->hasFnAttribute(llvm::Attribute::NoInline))
          F->removeFnAttr(llvm::Attribute::NoInline);
        if(!F->hasFnAttribute(llvm::Attribute::AlwaysInline))
          F->addFnAttr(llvm::Attribute::AlwaysInline);
        if(F->hasFnAttribute(llvm::Attribute::OptimizeNone))
          F->addFnAttr(llvm::Attribute::OptimizeNone);

        if (I->getCalledFunction()->getName().find("native8syscalls6printf") !=
            StringRef::npos) {
          //__dump(*I);
          //	if (auto ASC = dyn_cast<ConstantExpr>(I->getOperand(0)))
          {
            if (auto GEP = dyn_cast<ConstantExpr>(I->getOperand(0))) {
              if (auto str = dyn_cast<GlobalVariable>(GEP->getOperand(0))) {
                str->mutateType(
                    str->getType()->getPointerElementType()->getPointerTo(4));
                auto c0 =
                    ConstantInt::get(Type::getInt64Ty(M.getContext()), 0);
                vector<Value *> idx;
                idx.push_back(c0);
                idx.push_back(c0);
                auto newGEP = GetElementPtrInst::Create(
                    str->getType()->getElementType(), str, idx);
                auto ASC =
                    AddrSpaceCastInst::CreatePointerBitCastOrAddrSpaceCast(
                        newGEP,
                        newGEP->getType()->getSequentialElementType()->getPointerTo());
                I->setOperand(0, ASC);
                to_insert.push_back(
                    pair<Instruction *, Instruction *>(I, newGEP));
                to_insert.push_back(pair<Instruction *, Instruction *>(I, ASC));
              }
            }
          }
          //__dump(*I);
        }
      }
    });
    auto kernels = pacxx::getTagedFunctions(&M, "nvvm.annotations", "kernel");

    for (auto &F : kernels) {
      visitor.visit(F);

      for (auto &p : to_insert) {
        p.second->insertBefore(p.first);
      }

      to_insert.clear();
    }
#if 1
    cleanupDeadCode(&M);
#else

    std::vector<GlobalValue*> dead;
    for (auto &G : M.getGlobalList()) {
      if (G.getLinkage() != GlobalValue::LinkageTypes::ExternalLinkage)
        G.setLinkage(GlobalValue::LinkageTypes::InternalLinkage);
      if (!G.hasLocalLinkage()) {
        G.setVisibility(GlobalValue::VisibilityTypes::HiddenVisibility);
      }

      if(!isa<Function>(G) && G.hasNUses(0))
        dead.push_back(&G);

    }

    for (auto G : dead)
      G->eraseFromParent();

    for (auto &F : M) {
      if (std::find(kernels.begin(), kernels.end(), &F) == kernels.end()
          && !F.isDeclaration())
        F.setLinkage(GlobalValue::LinkageTypes::InternalLinkage);
    }
#endif
    return true;
  }
};

char PACXXCodeGenPrepare::ID = 0;
static RegisterPass<PACXXCodeGenPrepare>
    X("pacxx-codegen-prepare", "Prepares a Module for PACXX Code Generation", false,
      false);
}

namespace pacxx {
Pass *createPACXXCodeGenPrepare() { return new PACXXCodeGenPrepare(); }
}
