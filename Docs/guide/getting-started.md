# Getting Started with Glue

Welcome to **Glue** is a minimalist, statically typed procedural programming language that compiles directly to native binaries via LLVM.

This guide walks you through building the Glue compiler from source and compiling your first program.

---

## System Requirements

To build the Glue compiler, ensure you have the following installed on your machine:
- **C++ Compiler**: Supporting `C++20` standard (Clang or GCC)
- **CMake**: Version `3.20` or higher
- **LLVM**: Developer libraries, headers, and `clang` (LLVM 18+ / 22)

::: tip Installing LLVM on macOS (Homebrew)
```bash
brew install llvm cmake
```
The CMake build script automatically resolves the Homebrew LLVM path using `brew --prefix llvm`.
:::

---

## Building the Compiler

Clone the repository and compile the project using CMake:

```bash
mkdir build && cd build
cmake ../Glue
cmake --build .
```

Upon a successful build, the `glue` compiler executable will be available in your build directory.

---

## Your First Glue Program

Create a source file named `main.glue`:

```c
func int main() {
    shout("Hello from Glue!");
    return 0;
}
```

### Compiling and Running

Invoke the compiler by passing your source file:

```bash
./glue main.glue
```

::: info What happens under the hood?
1. The lexer and parser generate and validate the Abstract Syntax Tree (AST).
2. The semantic analyzer enforces types and mutability invariants.
3. The LLVM code generator emits Intermediate Representation into `glue.ll`.
4. The compiler invokes `clang` to produce the native executable `glue_exe` and runs it.
:::
