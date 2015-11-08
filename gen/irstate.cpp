//===-- irstate.cpp -------------------------------------------------------===//
//
//                         LDC – the LLVM D compiler
//
// This file is distributed under the BSD-style LDC license. See the LICENSE
// file for details.
//
//===----------------------------------------------------------------------===//

#include "gen/irstate.h"
#include "declaration.h"
#include "mtype.h"
#include "statement.h"
#include "gen/llvm.h"
#include "gen/tollvm.h"
#include "ir/irfunction.h"
#include <cstdarg>

IRState *gIR = 0;
llvm::TargetMachine *gTargetMachine = 0;
const llvm::DataLayout *gDataLayout = 0;
TargetABI *gABI = 0;

//////////////////////////////////////////////////////////////////////////////////////////
IRScope::IRScope() : builder(gIR->context()) { begin = NULL; }

IRScope::IRScope(llvm::BasicBlock *b) : begin(b), builder(b) {}

const IRScope &IRScope::operator=(const IRScope &rhs) {
  begin = rhs.begin;
  builder.SetInsertPoint(begin);
  return *this;
}

//////////////////////////////////////////////////////////////////////////////////////////
IRState::IRState(const char *name, llvm::LLVMContext &context)
    : module(name, context), DBuilder(this) {
  mutexType = NULL;
  moduleRefType = NULL;

  dmodule = 0;
  mainFunc = 0;
  ir.state = this;
  asmBlock = NULL;
}

IrFunction *IRState::func() {
  assert(!functions.empty() && "Function stack is empty!");
  return functions.back();
}

llvm::Function *IRState::topfunc() {
  assert(!functions.empty() && "Function stack is empty!");
  return functions.back()->func;
}

llvm::Instruction *IRState::topallocapoint() {
  assert(!functions.empty() && "AllocaPoint stack is empty!");
  return functions.back()->allocapoint;
}

IRScope &IRState::scope() {
  assert(!scopes.empty());
  return scopes.back();
}

llvm::BasicBlock *IRState::scopebb() {
  IRScope &s = scope();
  assert(s.begin);
  return s.begin;
}

bool IRState::scopereturned() {
  // return scope().returned;
  return !scopebb()->empty() && scopebb()->back().isTerminator();
}

LLCallSite IRState::CreateCallOrInvoke(LLValue *Callee, const char *Name) {
  LLSmallVector<LLValue *, 1> args;
  return func()->scopes->callOrInvoke(Callee, args, Name);
}

LLCallSite IRState::CreateCallOrInvoke(LLValue *Callee, LLValue *Arg1,
                                       const char *Name) {
  LLValue *args[] = {Arg1};
  return func()->scopes->callOrInvoke(Callee, args, Name);
}

LLCallSite IRState::CreateCallOrInvoke(LLValue *Callee, LLValue *Arg1,
                                       LLValue *Arg2, const char *Name) {
  LLValue *args[] = {Arg1, Arg2};
  return func()->scopes->callOrInvoke(Callee, args, Name);
}

LLCallSite IRState::CreateCallOrInvoke(LLValue *Callee, LLValue *Arg1,
                                       LLValue *Arg2, LLValue *Arg3,
                                       const char *Name) {
  LLValue *args[] = {Arg1, Arg2, Arg3};
  return func()->scopes->callOrInvoke(Callee, args, Name);
}

LLCallSite IRState::CreateCallOrInvoke(LLValue *Callee, LLValue *Arg1,
                                       LLValue *Arg2, LLValue *Arg3,
                                       LLValue *Arg4, const char *Name) {
  LLValue *args[] = {Arg1, Arg2, Arg3, Arg4};
  return func()->scopes->callOrInvoke(Callee, args, Name);
}

bool IRState::emitArrayBoundsChecks() {
  if (global.params.useArrayBounds != BOUNDSCHECKsafeonly) {
    return global.params.useArrayBounds == BOUNDSCHECKon;
  }

  // Safe functions only.
  if (functions.empty())
    return false;

  Type *t = func()->decl->type;
  return t->ty == Tfunction && ((TypeFunction *)t)->trust == TRUSTsafe;
}

//////////////////////////////////////////////////////////////////////////////////////////

IRBuilder<> *IRBuilderHelper::operator->() {
  IRBuilder<> &b = state->scope().builder;
  assert(b.GetInsertBlock() != NULL);
  return &b;
}
