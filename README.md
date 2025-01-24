<div align="center">

# SLJIT

Platform-independent low-level JIT compiler

https://zherczeg.github.io/sljit/

</div>

---

## 👉 Purpose

SLJIT is a low-level, platform-independent JIT compiler, which is very well suited for translating bytecode into machine code.

## ✨ Features

- Supports a variety of target architectures:
  - `x86` 32 / 64
  - `ARM` 32 / 64
  - `RISC-V` 32 / 64
  - `s390x` 64
  - `PowerPC` 32 / 64
  - `LoongArch` 64
  - `MIPS` 32 / 64
- Supports a large number of operations
    - Self-modifying code
    - Tail calls
    - Fast calls
    - Byte order reverse (endianness switching)
    - Unaligned memory accesses
    - SIMD
    - Atomic operations
- Allows direct access to registers (both integer and floating point)
- Supports stack space allocation for function local variables
- Supports all-in-one compilation
  - Allows SLJIT's API to be completely hidden from external use
- Allows serializing the compiler into a byte buffer
    - Useful for ahead-of-time (AOT) compilation
    - Code generation can be resumed after deserialization (partial AOT compilation)

## 🚀 Quickstart

Copy the `sljit_src` directory into your project's source directory and include [`sljitLir.h`](./sljit_src/sljitLir.h):

```c
#include "sljitLir.h"

typedef sljit_sw (SLJIT_FUNC *passthrough_func_t)(sljit_sw arg);

int main()
{
  void *code;
  passthrough_func_t func;

  /* Create the compiler */
  struct sljit_compiler *c = sljit_create_compiler(NULL);

  /* Create the function prologue */
  sljit_emit_enter(c,
      0,                 /* Options */
      SLJIT_ARGS1(W, W), /* 1 return value and 1 parameter of type sljit_sw */
      1,                 /* 1 scratch register used */
      1,                 /* 1 saved register used */
      0);                /* 0 bytes allocated for function local variables */

  /* The first (and in this case only) argument */
  /* is passed in register S0                   */

  /* Create the function epilogue, directly     */
  /* returning the value in S0                  */
  sljit_emit_return(c, SLJIT_MOV, SLJIT_S0, 0);

  /* Generate excutable machine code */
  code = sljit_generate_code(c, 0, NULL);

  /* Call the generated function */
  func = (passthrough_func_t) code;
  return func(17);
}
```

Grab a C99 compatible C/C++ compiler, build and link against [`sljitLir.c`](./sljit_src/sljitLir.c). Done!

## 📖 Documentation

SLJIT's documentation can be found on [its website](https://zherczeg.github.io/sljit/), as well as in the [`docs` folder](./docs/).

Also, take a look at [`sljitLir.h`](./sljit_src/sljitLir.h).

## 📨 Contact

Either open an [issue](https://github.com/zherczeg/sljit/issues) or write an email to hzmester@freemail.hu.

## ⚖️ License

SLJIT is licensed under the [Simplified BSD License](./LICENSE).

## ❤️ Special Thanks

- Alexander Nasonov
- Carlo Marcelo Arenas Belón
- Christian Persch
- Daniel Richard G.
- Giuseppe D'Angelo
- H.J. Lu
- James Cowgill
- Jason Hood
- Jiong Wang (*TileGX support*)
- Marc Mutz
- Martin Storsjö
- Michael McConville
- Mingtao Zhou (*LoongArch support*)
- Walter Lee
- Wen Xichang
- YunQiang Su
