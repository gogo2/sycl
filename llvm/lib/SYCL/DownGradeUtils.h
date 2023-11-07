//===- DownGradeUtils.h                                     ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Tools to down-grade IR
//
// ===---------------------------------------------------------------------===//

#include "llvm/IR/Attributes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Module.h"

namespace llvm {

inline void removeAttributes(Module &M, ArrayRef<Attribute::AttrKind> Kinds) {
  for (llvm::Function &F : M.functions())
    for (auto Kind : Kinds) {
      F.removeFnAttr(Kind);
      F.removeRetAttr(Kind);
      for (auto &P : F.args())
        P.removeAttr(Kind);
      for (User* U : F.users())
        if (CallBase *CB = dyn_cast<CallBase>(U)) {
          CB->removeAttributeAtIndex(AttributeList::FunctionIndex, Kind);
          CB->removeAttributeAtIndex(AttributeList::ReturnIndex, Kind);
          for (unsigned int i = 0; i < CB->arg_size(); ++i) {
            CB->removeParamAttr(i, Kind);
          }
        }
    }
}

inline void removeMetadata(Module &M, StringRef MetadataName) {
  llvm::NamedMDNode *Old = M.getOrInsertNamedMetadata(MetadataName);
  if (Old)
    M.eraseNamedMetadata(Old);
}

/// Replace the function named OldN by the function named NewN then delete the
/// function named OldN.
inline void replaceFunction(Module &M, StringRef OldN, StringRef NewN) {
  Function *Old = M.getFunction(OldN);
  Function *New = M.getFunction(NewN);
  if (!Old)
    return;
  assert(New);
  assert(Old->getFunctionType() == New->getFunctionType() &&
         "replacement is not possible");
  Old->replaceAllUsesWith(New);
  Old->eraseFromParent();
}

} // namespace llvm
