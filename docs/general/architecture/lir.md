---
sidebar_position: 3
---

# LIR

Defining a LIR which provides wide range of optimization opportunities and still can be efficiently translated to machine code on all CPUs is the biggest challenge of this project.
Those instruction forms and features which are supported on many (but not necessarily on all) architectures are carefully selected and a LIR is created from them.
These features are also emulated by the remaining architectures with low overhead.
For example, SLJIT supports various memory addressing modes and setting status register bits.

This approach is very effective for byte-code interpreters since their machine independent byte code (middle level representation) typically contains instructions which either can be easly translated to machine code, or which are not worth to translate to machine code in the first place.
