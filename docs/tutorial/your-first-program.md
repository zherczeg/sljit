---
sidebar_position: 2
---

# Your First Program

To use SLJIT, simply include the `sljitLir.h` header in your code and link against `sljitLir.c`.
Let's jump right into your first program:

import CodeBlock from '@theme/CodeBlock';
import FirstProgramSource from '!!raw-loader!./sources/first_program.c';

<CodeBlock language="c">{FirstProgramSource}</CodeBlock>

Code generation with SLJIT typically starts with `sljit_emit_enter`, which generates the *function prologue*: certain register are saved to the stack, stack space for function local variables is allocted if requested and optionally a call frame established.
