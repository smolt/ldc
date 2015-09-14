//===-- gen/abi-ppc-64.h - PPC64 ABI description ----------------*- C++ -*-===//
//
//                         LDC – the LLVM D compiler
//
// This file is distributed under the BSD-style LDC license. See the LICENSE
// file for details.
//
//===----------------------------------------------------------------------===//
//
// The ABI implementation used for iOS ARM64 (AArch64) targets.
//
//===----------------------------------------------------------------------===//

#ifndef LDC_GEN_ABI_IOS_ARM64_H
#define LDC_GEN_ABI_IOS_ARM64_H

struct TargetABI;

TargetABI* getIOSArm64TargetABI();

#endif
