//===-- abi-ios-arm64.cpp ---------------------------------------------------===//
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
struct CompositeToArray64 : ABIRewrite
{
    LLValue* get(Type* dty, DValue* dv)
    {
        Logger::println("rewriting i64 array -> as %s", dty->toChars());
        LLValue* rval = dv->getRVal();
        LLValue* lval = DtoRawAlloca(rval->getType(), 0);
        DtoStore(rval, lval);

        LLType* pTy = getPtrToType(DtoType(dty));
        return DtoLoad(DtoBitCast(lval, pTy), "get-result");
    }

    LLValue* put(Type* dty, DValue* dv)
    {
        Logger::println("rewriting %s -> as i64 array", dty->toChars());
        LLType* t = type(dty, nullptr);
        return DtoLoad(DtoBitCast(dv->getRVal(), getPtrToType(t)));
    }

    LLType* type(Type* t, LLType*)
    {
        // An i64 array that will hold Type 't'
        size_t sz = (t->size()+7)/8;
        return LLArrayType::get(LLIntegerType::get(gIR->context(), 64), sz);
    }
};
} // end local stuff

struct IOSArm64TargetABI : TargetABI
{
    CompositeToArray64 compositeToArray64;

    bool returnInArg(TypeFunction* tf)
    {
        if (tf->isref)
            return false;

        // Should be same rule as passByValue for args
        Type* rt = tf->next->toBasetype();
        // TODO: when structs returned in registers and saved in mem, pad may
        // be undefined which cause a problem for bit comparisons.  Punt for
        // now on C-ABI compatibity here so D internally works.
        // Will need more work anyway for HFAs.
        //return rt->ty == Tsarray || (rt->ty == Tstruct && rt->size() > 16);
        return rt->ty == Tsarray || rt->ty == Tstruct;
    }

    bool passByVal(Type* t)
    {
        // TODO: should Tsarray be treated as a HFA or HVA (up-to 4 elements)?
        t = t->toBasetype();
        return t->ty == Tsarray || (t->ty == Tstruct && t->size() > 16);
    }

    void rewriteFunctionType(TypeFunction* t, IrFuncTy &fty)
    {
        // TODO: does not seem to be rewriting variadic struct args
        // TODO:clang does a rewrite to an i64 or i128 type
        // we could to, or do as a i64 array
        for (IrFuncTy::ArgIter I = fty.args.begin(), E = fty.args.end(); I != E; ++I)
        {
            IrFuncTyArg& arg = **I;

            // skip if not passing full value
            if (arg.byref)
                continue;

            Type* ty = arg.type->toBasetype();
            if (ty->ty == Tstruct || ty->ty == Tsarray)
            {
                arg.rewrite = &compositeToArray64;
                arg.ltype = compositeToArray64.type(arg.type, arg.ltype);
            }
        }
    }
};

// The public getter for abi.cpp
TargetABI* getIOSArm64TargetABI()
{
    return new IOSArm64TargetABI();
}
