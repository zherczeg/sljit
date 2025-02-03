<div align="center">

# SLJIT

Platform-independent low-level JIT compiler

https://zherczeg.github.io/sljit/

</div>

---

## Purpose

SLJIT is a low-level, platform-independent JIT compiler, which is very well suited for translating bytecode into machine code.

## Features

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

## Documentation

SLJIT's documentation can be found on [its website](https://zherczeg.github.io/sljit/), as well as in the [`docs` folder](./docs/).

Also, take a look at [`sljitLir.h`](./sljit_src/sljitLir.h).

## Contact

Either open an [issue](https://github.com/zherczeg/sljit/issues) or write an email to hzmester@freemail.hu.

## License

SLJIT is licensed under the [Simplified BSD License](./LICENSE).

## Special Thanks

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
