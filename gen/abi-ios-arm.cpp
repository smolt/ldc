//===-- abi-ios.cpp -------------------------------------------------------===//
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

bool isStructIntegerLike(TypeStruct* t)
{
    // To be integer-like, all fields must be addressed at offset 0
    // (e.g. union or bit-fields) and must be integral type, pointer (we
    // extend to D pointer-ish types like class ref or AA), or another
    // integer-like struct.  clang's isIntegerLikeType() in TargetInfo.cpp
    // does something similar.

    VarDeclarations fields = t->sym->fields;
    for (size_t i = 0; i < fields.dim; ++i)
    {
        if (fields[i]->offset != 0)
            return false;

        Type* ft = fields[i]->type;
        if (!(ft->isintegral() || ft->ty == Tpointer ||
              ft->ty == Tclass || ft->ty == Taarray ||
              (ft->ty == Tstruct && isStructIntegerLike((TypeStruct*)ft))))
            return false;
    }

    return true;
}

bool isStructSimple(TypeStruct* t)
{
    // Is this struct simple?  From APCS 10.3.3 "a structure is considered
    // integer-like if its size is less than or equal to one word, and the
    // offset of each of its addressable subfields is zero. An integer-like
    // structured result is considered simple and is returned in register a1
    // [that is r0]."
    return (t->Type::size() <= 4 && isStructIntegerLike(t));
}

struct CompositeToArray32 : ABIRewrite
{
    LLValue* get(Type* dty, LLValue* v)
    {
        Logger::println("rewriting i32 array -> as %s", dty->toChars());
        //LLValue* rval = dv->getRVal();
        LLValue* rval = v;
        LLValue* lval = DtoRawAlloca(rval->getType(), 0);
        DtoStore(rval, lval);

        LLType* pTy = getPtrToType(DtoType(dty));
        return DtoLoad(DtoBitCast(lval, pTy), "get-result");
    }

    LLValue* put(DValue* dv)
    {
        Type* dty = dv->getType();
        Logger::println("rewriting %s -> as i32 array", dty->toChars());
        LLType* t = type(dty, nullptr);
        return DtoLoad(DtoBitCast(dv->getRVal(), getPtrToType(t)));
    }

    LLType* type(Type* t, LLType*)
    {
        // An i32 array that will hold Type 't'
        size_t sz = (t->size()+3)/4;
        return LLArrayType::get(LLIntegerType::get(gIR->context(), 32), sz);
    }
};
} // end local stuff

struct IOSArmTargetABI : TargetABI
{
    CompositeToArray32 compositeToArray32;

    bool returnInArg(TypeFunction* tf)
    {
        if (tf->isref)
            return false;

        // Normally return static arrays and structs in an sret arg, but need
        // to make an exception for "simple" integer-like structs to be
        // compatible with the C ABI.  APCS 10.3.3 says integer-like structs
        // should be returned in r0.  Doesn't work for extern(D) with non-POD
        // structs for some reason I haven't tracked down yet (failure in
        // std.algorithm.move when struct has a ctor).
        Type* rt = tf->next->toBasetype();
        if (tf->linkage == LINKd)
            return rt->ty == Tstruct || rt->ty == Tsarray;

        // C/C++ ABI (and others until we know better)
        return ((rt->ty == Tstruct && !isStructSimple((TypeStruct*)rt)) ||
                rt->ty == Tsarray);
    }

    bool passByVal(Type* t)
    {
        // Do not use llvm byval attribute as clang does not use for C ABI.
        // Plus there seems to be a llvm optimizer problem in the "top-down
        // list latency scheduler" pass that reorders instructions
        // incorrectly.
        return false;

        // the codegen is horrible for arrays passed by value - tries to do
        // copy without a loop for huge arrays.  Would be better if byval
        // could be used, but then there is the optimizer problem.  Maybe can
        // figure that out?
#if 0
        TY ty = t->toBasetype()->ty;
        return ty == Tsarray;
#endif
    }

    void rewriteFunctionType(TypeFunction* tf, IrFuncTy &fty)
    {
        for (IrFuncTy::ArgRIter I = fty.args.rbegin(), E = fty.args.rend(); I != E; ++I)
        {
            IrFuncTyArg& arg = **I;
            if (!arg.byref)
                rewriteArgument(fty, arg);
        }
    }

    void rewriteArgument(IrFuncTy& fty, IrFuncTyArg& arg)
    {
        // rewrite structs and arrays passed by value as llvm i32 arrays.
        // This keeps data layout unchanged when passed in arg registers r0-r3
        // and is necessary to match clang's C ABI for struct passing.
        // Without out this rewrite, each field or array element is passed in
        // own register.  For example: char[4] now all fits in r0, where
        // before it consumed r0-r3.
        Type* ty = arg.type->toBasetype();

        // TODO: want to also rewrite Tsarray as i32 arrays, but sometimes
        // llvm selects an aligned ldrd instruction even though the ptr is
        // unaligned (e.g. walking through members of array char[5][]).
        //if (ty->ty == Tstruct || ty->ty == Tsarray)
        if (ty->ty == Tstruct)
        {
            arg.rewrite = &compositeToArray32;
            arg.ltype = compositeToArray32.type(arg.type, arg.ltype);
        }
    }
};

TargetABI* getIOSArmTargetABI()
{
    return new IOSArmTargetABI;
}
