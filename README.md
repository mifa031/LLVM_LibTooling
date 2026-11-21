# HW2: LLVM

- `analyze` lists each function's name and counts its parameters, statements, loops, and calls in the input file.
- `strength-reduce` changes integer multiplication and division by powers of two into bit shifts.

## Install and build

On Ubuntu 24.04, run these commands from the repository root:

```sh
sudo apt install build-essential cmake clang-18 llvm-18-dev libclang-18-dev
cmake -S . -B build -DCMAKE_PREFIX_PATH=/usr/lib/llvm-18
cmake --build build
```

## Run

```sh
./build/analyze input.c -- -std=c11
./build/strength-reduce input.c -- -std=c11 > reduced.c
```

Replace `input.c` with your source file. For C++, use `-std=c++17`.
