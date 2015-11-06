//===-- gen/abi-ios-arm.h - iOS ARM ABI description -------------*- C++ -*-===//
//
//                         LDC – the LLVM D compiler
//
// This file is distributed under the BSD-style LDC license. See the LICENSE
// file for details.
//
//===----------------------------------------------------------------------===//
//
// The ABI implementation used for iOS ARM targets.
//
//===----------------------------------------------------------------------===//

#ifndef LDC_GEN_ABI_IOS_ARM_H
#define LDC_GEN_ABI_IOS_ARM_H

struct TargetABI;

TargetABI* getIOSArmTargetABI();

#endif
