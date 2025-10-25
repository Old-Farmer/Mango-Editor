# Mango

Mango is an modern terminal editor that focuses on simplicity and OOTB.

It's my hobby project. I really don't know how far I could go. Any advice is welcome.

## Target

Terminal editors are always difficult to use, hard to configure. Most of them were designed decades ago, so they have complex key bindings, wierd interfaces and need tons of community plugins to become user-friendly. This project aims to make users life easier --- no more high learning curves, no more tons of plugins, no more hundreds lines of configuration. All just out of box. Having fun with codes!

## Note

Linux only now.

This project is far from mature. Do not use it in the production environment.

## Features

- utf-8 support without grapheme cluster
- mouse support
- simple edit
- simple syntax highlighting with tree-sitter(only a few languages now)

See docs/help.txt for more infomation

## Build

This project uses CMake build system.

Requirements:

1. A C++ compiler which supports C++17
2. CMake >= 3.22
3. make or ninja

```bash
# Download/Update tree-sitter grammars
./download-tree-sitter-grammars.sh third-party/tree-sitter-grammars

# Debug
mkdir build && cd build && cmake -DCMAKE_BUILD_TYPE=Debug .. && make -j4
# Release
mkdir build && cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && make -j4

# Execute app
./mango
# Execute test
./mango_test
```
