//===-----------------------------------------------------------*- C++ -*-===//
//
//                       The LLVM-based PACXX Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#pragma once

#include <llvm/ADT/SmallVector.h>

namespace llvm {
class Pass;
}

namespace pacxx {
llvm::Pass *createIntrinsicSchedulerPass();
llvm::Pass *createMemoryCoalescingPass(bool);
llvm::Pass *createIntrinsicMapperPass();
llvm::Pass *createMSPGenerationPass();
llvm::Pass *createMSPCleanupPass();
llvm::Pass *createMSPRemoverPass();
llvm::Pass *createTargetSelectionPass(const llvm::SmallVector<std::string, 2>& targets);
llvm::Pass *createPACXXCodeGenPrepare();
llvm::Pass *createLoadMotionPass();
llvm::Pass *createAddressSpaceTransformPass();
}