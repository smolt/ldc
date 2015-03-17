//===-- abi-ios.cpp -------------------------------------------------------===//
//
//                         LDC – the LLVM D compiler
//
// This file is distributed under the BSD-style LDC license. See the LICENSE
// file for details.
//
//===----------------------------------------------------------------------===//

#include "gen/abi.h"
#include "gen/abi-generic.h"
#include "gen/abi-ios.h"

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
            // TODO: if we want to match the iOS C abi for D calls.
            //return llvm::CallingConv::C;
        default:
            llvm_unreachable("Unhandled D linkage type.");
        }
    }

    bool returnInArg(TypeFunction* tf)
    {
        // TODO: this needs to be fixed to return in r0 anything that fits in
        // 32-bits 
        if (tf->isref)
            return false;

        // Return structs and static arrays on the stack. The latter is needed
        // because otherwise LLVM tries to actually return the array in a number
        // of physical registers, which leads, depending on the target, to
        // either horrendous codegen or backend crashes.
        Type* rt = tf->next->toBasetype();
        return (rt->ty == Tstruct || rt->ty == Tsarray);
        //return false;
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
