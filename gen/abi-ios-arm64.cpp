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
#if 0
// A Homogeneous Floating-point Aggregate (HFA) consists of up to 4 of same
// floating point type (all float or all double). The iOS C-ABI considers real
// (long double) same as double.  Also consider D floats of same size as same
// float type (e.g. ifloat and float are same).  It is the aggregate final
// data layout that matters so nested structs, unions, and sarrays can result
// in an HFA.
//
// simple HFAs: struct F1 {float f;}  struct D4 {double a,b,c,d;}
// interesting HFA: struct {F1[2] vals; float weight;}

bool isNestedHFA(const TypeStruct *t, d_uns64 &floatSize, int &num,
                 uinteger_t adim) {
  // Used internally by isHFA().

  // Return true if struct 't' is part of an HFA where 'floatSize' is sizeof
  // the float and 'num' is number of these floats so far.  On return, 'num'
  // is updated to the total number of floats in the HFA.  Set 'floatSize'
  // to 0 discover the sizeof the float.  When struct 't' is part of an
  // sarray, 'adim' is the dimension of that array, otherwise it is 1.
  VarDeclarations fields = t->sym->fields;

  // HFA can't contains an empty struct
  if (fields.dim == 0)
    return false;

  // Accumulate number of floats in HFA
  int n;

  // For unions, need to find field with most floats
  int maxn = num;

  for (size_t i = 0; i < fields.dim; ++i) {
    Type *field = fields[i]->type;

    // reset to initial num floats (all union fields are at offset 0)
    if (fields[i]->offset == 0)
      n = num;

    // reset dim to dimension of sarray we are in (will be 1 if not)
    uinteger_t dim = adim;

    // Field is an array.  Process the arrayof type and multiply dim by
    // array dim.  Note that empty arrays immediately exclude this struct
    // from HFA status.
    if (field->ty == Tsarray) {
      TypeSArray *array = (TypeSArray *)field;
      if (array->dim->toUInteger() == 0)
        return false;
      field = array->nextOf();
      dim *= array->dim->toUInteger();
    }

    if (field->ty == Tstruct) {
      if (!isNestedHFA((TypeStruct *)field, floatSize, n, dim))
        return false;
    } else if (field->isfloating()) {
      d_uns64 sz = field->size();
      n += dim;

      if (field->iscomplex()) {
        sz /= 2; // complex is 2 floats, adjust sz
        n += dim;
      }

      if (floatSize == 0) // discovered floatSize
        floatSize = sz;
      else if (sz != floatSize) // different float size, reject
        return false;

      if (n > 4)
        return false; // too many floats for HFA, reject
    } else {
      return false; // reject all other types
    }

    if (n > maxn)
      maxn = n;
  }

  num = maxn;
  return true;
}

bool isHFA(TypeStruct *t, LLType **rewriteType = nullptr) {
  // Check if struct 't' is an HFA.  If so, optionally produce the
  // rewriteType: an array of floats
  d_uns64 floatSize = 0;
  int num = 0;

  if (isNestedHFA(t, floatSize, num, 1)) {
    if (rewriteType) {
      LLType *floatType = nullptr;
      switch (floatSize) {
      case 4:
        floatType = llvm::Type::getFloatTy(gIR->context());
        break;
      case 8:
        floatType = llvm::Type::getDoubleTy(gIR->context());
        break;
      default:
        llvm_unreachable("Unexpected size for float type");
      }
      *rewriteType = LLArrayType::get(floatType, num);
    }
    return true;
  }
  return false;
}

// Rewrite HFAs as array of float type
struct HFAToArray : ABIRewrite {
  LLValue *get(Type *dty, LLValue *v) {
    Logger::println("rewriting array -> as HFA %s", dty->toChars());
    LLValue *lval = DtoRawAlloca(v->getType(), 0);
    DtoStore(v, lval);

    LLType *pTy = getPtrToType(DtoType(dty));
    return DtoLoad(DtoBitCast(lval, pTy), "get-result");
  }

  LLValue *put(DValue *dv) {
    Type *dty = dv->getType();
    Logger::println("rewriting HFA %s -> as array", dty->toChars());
    LLType *t = type(dty, nullptr);
    return DtoLoad(DtoBitCast(dv->getRVal(), getPtrToType(t)));
  }

  LLType *type(Type *ty, LLType *) {
    LLType *floatArrayType = nullptr;
    if (isHFA((TypeStruct *)ty, &floatArrayType))
      return floatArrayType;
    llvm_unreachable("Type ty should always be an HFA");
  }
};

// Rewrite composite as array of i64
struct CompositeToArray64 : ABIRewrite {
  LLValue *get(Type *dty, LLValue *v) {
    Logger::println("rewriting i64 array -> as %s", dty->toChars());
    LLValue *lval = DtoRawAlloca(v->getType(), 0);
    DtoStore(v, lval);

    LLType *pTy = getPtrToType(DtoType(dty));
    return DtoLoad(DtoBitCast(lval, pTy), "get-result");
  }

  LLValue *put(DValue *dv) {
    Type *dty = dv->getType();
    Logger::println("rewriting %s -> as i64 array", dty->toChars());
    LLType *t = type(dty, nullptr);
    return DtoLoad(DtoBitCast(dv->getRVal(), getPtrToType(t)));
  }

  LLType *type(Type *t, LLType *) {
    // An i64 array that will hold Type 't'
    size_t sz = (t->size() + 7) / 8;
    return LLArrayType::get(LLIntegerType::get(gIR->context(), 64), sz);
  }
};
#endif

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

  bool returnInArg(TypeFunction *tf) {
    if (tf->isref)
      return false;

    // Should be same rule as passByValue for args
    Type *rt = tf->next->toBasetype();

    // TODO: when structs returned in registers and saved in mem, pad may
    // be undefined which cause a problem for bit comparisons.  Punt for
    // now on using C-ABI for D here.
    if (tf->linkage == LINKd)
      return rt->ty == Tsarray || rt->ty == Tstruct;

    return rt->ty == Tsarray ||
           (rt->ty == Tstruct && rt->size() > 16 && !isHFA((TypeStruct *)rt));
  }

  bool passByVal(Type *t) {
    // TODO: Even though C-ABI doesn't deal with sarrays, should we treat
    // same as structs?
    t = t->toBasetype();
    return t->ty == Tsarray ||
           (t->ty == Tstruct && t->size() > 16 && !isHFA((TypeStruct *)t));
  }

  void rewriteFunctionType(TypeFunction *tf, IrFuncTy &fty) {
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

  void rewriteArgument(IrFuncTy &fty, IrFuncTyArg &arg) {
    Type *ty = arg.type->toBasetype();
    if (ty->ty == Tstruct || ty->ty == Tsarray) {
      if (ty->ty == Tstruct && isHFA((TypeStruct *)ty)) {
        arg.rewrite = &hfaToArray;
        arg.ltype = hfaToArray.type(arg.type, arg.ltype);
      } else {
        arg.rewrite = &compositeToArray64;
        arg.ltype = compositeToArray64.type(arg.type, arg.ltype);
      }
    }
  }

  void rewriteVarargs(IrFuncTy &fty, std::vector<IrFuncTyArg *> &args) {
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
