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
#include "gen/abi-ios.h"

// local stuff
namespace {

bool isStructIntegerLike(TypeStruct* t)
{
    // To be integer-like, all fields must be addressed at offset 0
    // (e.g. union or bit-fields) and must be integral type, pointer (we
    // extend to D pointer-ish types like class ref or AA), or another
    // integer-like struct.  clang's isIntegerLikeType() in TargetInfo.cpp
    // does something similar.
    //
    // note: no need to check size or isPOD again because those can't
    // change.

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
    // [that is r0]."  This should only applies to D "POD" structs (C
    // compatible).
    return (t->Type::size() <= 4 &&
            t->sym->isPOD() &&
            isStructIntegerLike(t));
}
} // end local stuff

struct IOSTargetABI : TargetABI
{
    llvm::CallingConv::ID callingConv(LINK l)
    {
        switch (l)
        {
        case LINKc:
        case LINKcpp:
        case LINKpascal:
        case LINKwindows:
            return llvm::CallingConv::C;
        case LINKd:
        case LINKdefault:
            return llvm::CallingConv::Fast;
        default:
            llvm_unreachable("Unhandled D linkage type.");
        }
    }

    bool returnInArg(TypeFunction* tf)
    {
        if (tf->isref)
            return false;

        // Normally return static arrays and structs in an sret arg, but need
        // to make an exception for "simple" integer-like structs to be
        // compatible with the C ABI.  APCS 10.3.3 says integer-like structs
        // should be returned in r0.
        Type* rt = tf->next->toBasetype();
        return ((rt->ty == Tstruct && !isStructSimple((TypeStruct*)rt)) ||
                rt->ty == Tsarray);
    }

    bool passByVal(Type* t)
    {
        return false;
    }

    void rewriteFunctionType(TypeFunction* t, IrFuncTy &fty)
    {
    }
};

TargetABI* getIOSTargetABI()
{
    return new IOSTargetABI;
}
