//===-- abi-ios-arm64.cpp -------------------------------------------------===//
//
//                         LDC – the LLVM D compiler
//
// This file is distributed under the BSD-style LDC license. See the LICENSE
// file for details.
//
//===----------------------------------------------------------------------===//
//
// The Procedure Call Standard can be found here:
// https://developer.apple.com/library/ios/documentation/Xcode/Conceptual/iPhoneOSABIReference/Articles/ARM64FunctionCallingConventions.html
//
// and here:
// http://infocenter.arm.com/help/topic/com.arm.doc.ihi0055b/IHI0055B_aapcs64.pdf
//
//===----------------------------------------------------------------------===//

#include "gen/abi.h"
#include "gen/abi-generic.h"
#include "gen/abi-ios-arm64.h"

namespace {
// Rewrites a composite as an integer of the same size.
struct CompositeToInt : ABIRewrite {
  LLValue *get(Type *dty, LLValue *v) {
    Logger::println("rewriting integer -> %s", dty->toChars());
    LLValue *mem = DtoAlloca(dty, ".int_to_composite");
    DtoStore(v, DtoBitCast(mem, getPtrToType(v->getType())));
    return DtoLoad(mem);
  }

  void getL(Type *dty, LLValue *v, LLValue *lval) {
    Logger::println("rewriting integer -> %s", dty->toChars());
    DtoStore(v, DtoBitCast(lval, getPtrToType(v->getType())));
  }

  LLValue *put(DValue *dv) {
    Type *dty = dv->getType();
    Logger::println("rewriting %s -> integer", dty->toChars());
    LLType *t = LLIntegerType::get(gIR->context(), dty->size() * 8);
    return DtoLoad(DtoBitCast(dv->getRVal(), getPtrToType(t)));
  }

  LLType *type(Type *t, LLType *) {
    size_t sz = t->size() * 8;
    return LLIntegerType::get(gIR->context(), sz);
  }
};
} // end local stuff

struct IOSArm64TargetABI : TargetABI {
  CompositeToArray64 compositeToArray64;
  HFAToArray hfaToArray;
  IntegerRewrite integerRewrite;
  // IntegerRewrite doesn't do i128, so bring back CompositeToInt
  CompositeToInt compositeToInt;
  ExplicitByvalRewrite byvalRewrite;

  IOSArm64TargetABI() : byvalRewrite(1) {}

  bool returnInArg(TypeFunction *tf) override {
    if (tf->isref)
      return false;

    // Should be same rule as passByValue for args
    Type *rt = tf->next->toBasetype();

    // When AAPCS64 returns a struct in registers, struct padding may be
    // undefined which causes a problem for bit comparisons.  Punt for now on
    // using C-ABI for D here.
    if (tf->linkage == LINKd && rt->ty == Tstruct) {
      return true;
    }

    // return aggregates > 16 bytes in arg, except HFAs
    // TODO: Tsarrays can be HFAs too, consider revising.
    return rt->size() > 16 &&
      (rt->ty == Tsarray || (rt->ty == Tstruct && !isHFA((TypeStruct *)rt)));
  }

  bool passByVal(Type *t) override {
    // byval in backend is not used for this target
    return false;
  }

  void rewriteFunctionType(TypeFunction *tf, IrFuncTy &fty) override {
    // Value struct returns should be rewritten as an int type to generate
    // correct register usage.  HFA struct returns don't normally need to
    // be rewritten (clang does not rewrite), but D unions don't seem to
    // match C unions when first member is not largest (maybe that is a
    // bug?), so rewrite HFAs anyway.
    //
    // note: sret functions change ret type to void so this won't trigger
    // for those
    Type *retTy = fty.ret->type->toBasetype();
    if (!fty.ret->byref && retTy->ty == Tstruct) {
      if (isHFA((TypeStruct *)retTy)) {
        fty.ret->rewrite = &hfaToArray;
        fty.ret->ltype = hfaToArray.type(fty.ret->type, fty.ret->ltype);
      } else {
        fty.ret->rewrite = &compositeToInt;
        fty.ret->ltype = compositeToInt.type(fty.ret->type, fty.ret->ltype);
      }
    }

    for (auto arg : fty.args) {
      if (!arg->byref)
        rewriteArgument(fty, *arg);
    }
  }

  void rewriteArgument(IrFuncTy &fty, IrFuncTyArg &arg) override {
    Type *ty = arg.type->toBasetype();
    if (ty->ty == Tstruct || ty->ty == Tsarray) {
      if (ty->ty == Tstruct && isHFA((TypeStruct *)ty)) {
        arg.rewrite = &hfaToArray;
        arg.ltype = hfaToArray.type(arg.type, arg.ltype);
      } else if (ty->size() > 16) {
        arg.rewrite = &byvalRewrite;
        arg.ltype = arg.ltype->getPointerTo();
      } else {
        arg.rewrite = &compositeToArray64;
        arg.ltype = compositeToArray64.type(arg.type, arg.ltype);
      }
    }
  }

  // TODO: revisit with an ABI test to see if we need to do the byvalRewrite
  // as above.  Need an abi test
  void rewriteVarargs(IrFuncTy &fty, std::vector<IrFuncTyArg *> &args) override {
    for (unsigned i = 0; i < args.size(); ++i) {
      IrFuncTyArg &arg = *args[i];
      if (!arg.byref) // don't rewrite ByVal arguments
      {
        // LLVM CallingConv::C promotes a varag float to double.
        // extern(D) wants it to remain a float.  I am not sure if
        // this is an LLVM bug or just behavior not encountered in C
        // where all vararg floats are promoted to double by the
        // frontend (backend never sees).
        switch (arg.type->toBasetype()->ty) {
        case Tfloat32:
        case Timaginary32:
          arg.rewrite = &integerRewrite;
          arg.ltype = integerRewrite.type(arg.type, arg.ltype);
          break;
        default:
          rewriteArgument(fty, arg);
          break;
        }
      }
    }
  }
};

// The public getter for abi.cpp
TargetABI *getIOSArm64TargetABI() { return new IOSArm64TargetABI(); }
