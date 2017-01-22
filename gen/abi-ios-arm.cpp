//===-- abi-ios-arm.cpp ---------------------------------------------------===//
//
//                         LDC – the LLVM D compiler
//
// This file is distributed under the BSD-style LDC license. See the LICENSE
// file for details.
//
//===----------------------------------------------------------------------===//

/*
  The iOS ARM ABI is based on a variant of the older APCS:

  http://infocenter.arm.com/help/topic/com.arm.doc.dui0041c/DUI0041C.pdf

  It is highly confusing because the iOS docummentation explicitly refers to
  the AAPCS in armv6 section, but clang source and LLVM mail lists says
  otherwise.

  https://developer.apple.com/library/ios/documentation/Xcode/Conceptual/iPhoneOSABIReference/Introduction/Introduction.html

*/

#include "gen/abi.h"
#include "gen/abi-generic.h"
#include "gen/abi-ios-arm.h"

// local stuff
namespace {

bool isStructIntegerLike(TypeStruct *t) {
  // To be integer-like, all fields must be addressed at offset 0
  // (e.g. union or bit-fields) and must be integral type, pointer (we
  // extend to D pointer-ish types like class ref or AA), or another
  // integer-like struct.  clang's isIntegerLikeType() in TargetInfo.cpp
  // does something similar.

  VarDeclarations fields = t->sym->fields;
  for (size_t i = 0; i < fields.dim; ++i) {
    if (fields[i]->offset != 0)
      return false;

    Type *ft = fields[i]->type;
    if (!(ft->isintegral() || ft->ty == Tpointer || ft->ty == Tclass ||
          ft->ty == Taarray ||
          (ft->ty == Tstruct && isStructIntegerLike((TypeStruct *)ft))))
      return false;
  }

  return true;
}

bool isStructSimple(TypeStruct *t) {
  // Is this struct simple?  From APCS 10.3.3 "a structure is considered
  // integer-like if its size is less than or equal to one word, and the
  // offset of each of its addressable subfields is zero. An integer-like
  // structured result is considered simple and is returned in register a1
  // [that is r0]."
  return (t->Type::size() <= 4 && isStructIntegerLike(t));
}

// Hacked in to do ARM APCS byval rewrites like clang with correct alignment.
// This is based on something similar in abi-x86-64.cpp
struct ImplicitByvalRewrite : ABIRewrite {
  LLValue *get(Type *dty, LLValue *v) override {
    return DtoLoad(v, ".ImplicitByvalRewrite_getResult");
  }

  void getL(Type *dty, LLValue *v, LLValue *lval) override {
    DtoMemCpy(lval, v);
  }

  LLValue *put(DValue *v) override {
    // if v alignment is good enough (ACPS iOS says 4-byte alignment), can use
    // as is, otherwise need to make a copy. Note that clang also makes a copy
    // if v is locatd in a different address space, which we are ignoring here.
    Type *dty = v->getType();
    const unsigned align = DtoAlignment(dty);
    if (align >= 4) {
      return getAddressOf(v);
    }
    
    LLValue *originalPointer = v->getRVal();
    LLType *type = originalPointer->getType()->getPointerElementType();
    LLValue *copyForCallee =
      DtoRawAlloca(type, 4, ".ImplicitByvalRewrite_putResult");
    DtoMemCpy(copyForCallee, originalPointer);
    return copyForCallee;
  }

  LLType *type(Type *dty, LLType *t) override { return DtoPtrToType(dty); }
};

} // end local stuff

struct IOSArmTargetABI : TargetABI {
  CompositeToArray32 compositeToArray32;
  ImplicitByvalRewrite byvalRewrite;
  
  bool returnInArg(TypeFunction *tf) override {
    // Return composites in an arg, however APCS 10.3.3 says simple
    // integer-like structs should be returned in r0.  Doesn't apply to
    // non-POD structs.
    if (tf->isref)
      return false;

    Type *rt = tf->next->toBasetype();
    if (!isPOD(rt))
      return true;

    return ((rt->ty == Tstruct && !isStructSimple((TypeStruct *)rt)) ||
            rt->ty == Tsarray);
  }

  bool passByVal(Type *t) override {
    // APCS does not use an indirect arg to pass aggregates, however
    // clang uses byval for types > 64-bytes, then llvm backend
    // converts back to non-byval.  Without this special handling the
    // optimzer generates bad code (e.g. std.random unittest crash).

    // TODO: Using below as is in abi-arm wasn't setting up proper alignment
    // for iOS (needs to be byval with align 4).  Revised to use
    // ImplicitByvalRewrite instead.  Can clean this up after determining if
    // abi-arm.cpp is ok or needs this too.
#if 0
    t = t->toBasetype();
    return ((t->ty == Tsarray || t->ty == Tstruct) && t->size() > 64);
#else    
    return false;
#endif

// the codegen is horrible for arrays passed by value - tries to do
// copy without a loop for huge arrays.  Would be better if byval
// could be used, but then there is the optimizer problem.  Maybe can
// figure that out?
#if 0
        TY ty = t->toBasetype()->ty;
        return ty == Tsarray;
#endif
  }

  void rewriteFunctionType(TypeFunction *tf, IrFuncTy &fty) override {
    for (auto arg : fty.args) {
      if (!arg->byref)
        rewriteArgument(fty, *arg);
    }

    // extern(D): reverse parameter order for non variadics, for DMD-compliance
    if (tf->linkage == LINKd && tf->varargs != 1 && fty.args.size() > 1) {
      fty.reverseParams = true;
    }
  }

  void rewriteArgument(IrFuncTy &fty, IrFuncTyArg &arg) override {
    // structs and arrays need rewrite as i32 arrays.  This keeps data layout
    // unchanged when passed in registers r0-r3 and is necessary to match C ABI
    // for struct passing.  Without out this rewrite, each field or array
    // element is passed in own register.  For example: char[4] now all fits in
    // r0, where before it consumed r0-r3.
    Type *ty = arg.type->toBasetype();

    // TODO: want to also rewrite Tsarray as i32 arrays, but sometimes
    // llvm selects an aligned ldrd instruction even though the ptr is
    // unaligned (e.g. walking through members of array char[5][]).
    // if (ty->ty == Tstruct || ty->ty == Tsarray)
    if (ty->ty == Tstruct) {
      if (ty->size() > 64) {
        arg.rewrite = &byvalRewrite;
        arg.ltype = arg.ltype->getPointerTo();
        arg.attrs.addByVal(4);
      } else {
        arg.rewrite = &compositeToArray32;
        arg.ltype = compositeToArray32.type(arg.type, arg.ltype);
      }
    }
  }
};

TargetABI *getIOSArmTargetABI() { return new IOSArmTargetABI; }
