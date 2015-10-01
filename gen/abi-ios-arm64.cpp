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

bool isHFA(TypeStruct* t)
{
    // Homogeneous Floating-point Aggregates consists of up to 4 of same
    // floating point type. The ABI considers real and double the same type.
    // It is the aggregate final layout that matters so nested structs,
    // unions, and sarrays.  For now though, just doing the simple case of a
    // single struct with 4 matching floating point types.  It picks up the
    // most common cases.  TODO: finish

    VarDeclarations fields = t->sym->fields;
    if (fields.dim == 0 || fields.dim > 4) return false;

    // a floating type?  Skip complex types for now
    Type* ft = fields[0]->type;
    if (!ft->isfloating() || ft->iscomplex())
        return false;

    // See if all fields are same floating point size
    d_uns64 sz = ft->size();
    for (size_t i = 1; i < fields.dim; ++i)
    {
        Type* fti = fields[i]->type;
        if (!fti->isfloating() || fti->iscomplex())
            return false;
        if (fti->size() != sz)
            return false;
    }

    return true;
}

struct CompositeToHFA : ABIRewrite
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

    LLType* type(Type* ty, LLType*)
    {
        TypeStruct* t = (TypeStruct*)ty;
        VarDeclarations fields = t->sym->fields;
        Type* floatType = fields[0]->type;
        LLType* elty = DtoType(floatType);
        return LLArrayType::get(elty, fields.dim);
    }
};

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

/**
 * Rewrites a composite type parameter to an integer of the same size.
 *
 * This is needed in order to be able to use LLVM's inreg attribute to put
 * struct and static array parameters into registers, because the attribute has
 * slightly different semantics. For example, LLVM would store a [4 x i8] inreg
 * in four registers (zero-extended), instead of a single 32bit one.
 *
 * The LLVM value in dv is expected to be a pointer to the parameter, as
 * generated when lowering struct/static array paramters to LLVM byval.
 */
struct CompositeToInt : ABIRewrite
{
    LLValue* get(Type* dty, DValue* dv)
    {
        Logger::println("rewriting integer -> %s", dty->toChars());
        LLValue* mem = DtoAlloca(dty, ".int_to_composite");
        LLValue* v = dv->getRVal();
        DtoStore(v, DtoBitCast(mem, getPtrToType(v->getType())));
        return DtoLoad(mem);
    }

    void getL(Type* dty, DValue* dv, llvm::Value* lval)
    {
        Logger::println("rewriting integer -> %s", dty->toChars());
        LLValue* v = dv->getRVal();
        DtoStore(v, DtoBitCast(lval, getPtrToType(v->getType())));
    }

    LLValue* put(Type* dty, DValue* dv)
    {
        Logger::println("rewriting %s -> integer", dty->toChars());
        LLType* t = LLIntegerType::get(gIR->context(), dty->size() * 8);
        return DtoLoad(DtoBitCast(dv->getRVal(), getPtrToType(t)));
    }

    LLType* type(Type* t, LLType*)
    {
        size_t sz = t->size() * 8;
        return LLIntegerType::get(gIR->context(), sz);
    }
};
} // end local stuff

struct IOSArm64TargetABI : TargetABI
{
    CompositeToArray64 compositeToArray64;
    CompositeToHFA compositeToHFA;
    // IntegerRewrite doesn't do i128, so bring back CompositeToInt
    //IntegerRewrite integerRewrite;
    CompositeToInt compositeToInt;

    bool returnInArg(TypeFunction* tf)
    {
        if (tf->isref)
            return false;

        // Should be same rule as passByValue for args
        Type* rt = tf->next->toBasetype();

        // TODO: when structs returned in registers and saved in mem, pad may
        // be undefined which cause a problem for bit comparisons.  Punt for
        // now on using C-ABI for D here.
        if (tf->linkage == LINKd)
            return rt->ty == Tsarray || rt->ty == Tstruct;

        return rt->ty == Tsarray ||
            (rt->ty == Tstruct && rt->size() > 16 && !isHFA((TypeStruct*)rt));
    }

    bool passByVal(Type* t)
    {
        // TODO: should Tsarray be treated as a HFA or HVA?
        t = t->toBasetype();
        return t->ty == Tsarray ||
            (t->ty == Tstruct && t->size() > 16 && !isHFA((TypeStruct*)t));
    }

    void rewriteFunctionType(TypeFunction* tf, IrFuncTy &fty)
    {
        // value struct returns should be rewritten as an int type to generate
        // correct register usage.  HFA returns don't need to be rewritten
        // however (matches clang).
        // note: sret functions change ret type to void so
        // this won't trigger for those
        Type* retTy = fty.ret->type->toBasetype();
        if (!fty.ret->byref && retTy->ty == Tstruct && !isHFA((TypeStruct*)retTy))
        {
            fty.ret->rewrite = &compositeToInt;
            fty.ret->ltype = compositeToInt.type(fty.ret->type, fty.ret->ltype);
        }

        for (IrFuncTy::ArgIter I = fty.args.begin(), E = fty.args.end(); I != E; ++I)
        {
            IrFuncTyArg& arg = **I;
            if (!arg.byref)
                rewriteArgument(fty, arg);
        }
    }

    void rewriteArgument(IrFuncTy& fty, IrFuncTyArg& arg)
    {
        Type* ty = arg.type->toBasetype();
        if (ty->ty == Tstruct || ty->ty == Tsarray)
        {
            if (ty->ty == Tstruct && isHFA((TypeStruct*)ty))
            {
                arg.rewrite = &compositeToHFA;
                arg.ltype = compositeToHFA.type(arg.type, arg.ltype);
            }
            else
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
