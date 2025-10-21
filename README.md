# Mango

Mango is an modern terminal editor that focuses on simplicity and OOTB.

It's my hobby project. I really don't know how far I could go. Any advice is welcome.

## Note

Linux only now.

This project is far from mature. Do not use it in the production environment.

## Features

- utf-8 support without grapheme cluster
- mouse support
- simple edit

See docs/help.txt for more infomation

## Build

This project uses CMake build system.

Requirements:

1. A C++ compiler which supports C++17
2. CMake 3.22+
3. make

```bash
# Debug
mkdir build && cmake -DCMAKE_BUILD_TYPE=Debug .. && make -j4
# Release
mkdir build && cmake -DCMAKE_BUILD_TYPE=Release .. && make -j4

# Execute app
./mango
# Execute test
./mango_test
```
