# The LLVM Compiler for RISC-V with Xvec Extension Support

## Author

* André Bannwart Perina
* Guilherme Bileki

## Introduction

This is a fork from riscv-llvm project (https://github.com/riscv/riscv-llvm), featuring pattern
detection and substitution for simple for loops, making the use of Xvec vector instructions. An
implementation of an Xvec-enabled RISC-V processor is available at
https://github.com/comododragon/vscale.

## Licence

This project holds the original licence from the original project (see LICENSE file).

## Installation

```
$ git clone -b riscv-trunk https://github.com/riscv/riscv-llvm.git
$ git submodule update --init
$ mkdir build
$ cd build
$ cmake -DCMAKE_INSTALL_PREFIX=/path/to/install -DLLVM_TARGETS_TO_BUILD="RISCV" ../
$ make
$ make install
```

Where ```/path/to/install``` is your desired installation folder.

### Note

Although the RISC-V LLVM includes Clang, the RISC-V GNU Toolchain
(https://github.com/comododragon/riscv-gnu-toolchain) is still needed to perform assembly and linking.

Refer to the original repository documentation for further instructions on how to compile and use.

## Usage Information

Information on how substitution works will be announced soon.
