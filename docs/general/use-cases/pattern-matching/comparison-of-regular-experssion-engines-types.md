---
sidebar_position: 2
---

# Comparison of Regular Expression Engine Types

Matching regular expressions is not considered a complex task and many people think that this problem was solved a long time ago. The reason for this belief is a great deal of misinformation which has been spread across the internet and the education. On the contrary there is no best solution for matching a pattern at the moment, and choosing the right regular expression engine is as difficult as choosing the right programming language. The aim of this page is showing the advantages and disadvantages of different engine types.

In general we distinguish two engine types:
- **Performance-oriented engines:** very fast in general, and very slow in special (called pathological) cases.
- **Balanced engines:** no weaknesses, no strengths. Usually predictable worst case runtime.

Balanced engines are slower than performance-oriented engines, but adding some pathological patterns to the benchmark set can quickly turn the tide.

The following sections go into more detail regarding the different engine types from a technical point of view.

## Multiple Choice Engine with Backtracking

**Type:** Usually performance oriented engines

**Advantages:**
- Large feature set (assertions, conditional blocks, executing code blocks, backtracking control)
- Submatch capture
- Lots of optimization opportunities (mostly backtracking elimination)

**Disadvantages:**
- Large and complex code base
- May use a large amount of stack
- Examples for pathological cases:
    - `(a*)*b`
    - `(?:a?){N}a{N}` (where `N` is a positive integer number)

**Execution modes:** depth-first search algorithm
- Interpreted execution of a Non-deterministic Finite Automaton (NFA); example: [PCRE interpreter](https://www.pcre.org/)
- Generating machine code from an NFA; example: [Irregexp engine](https://blog.chromium.org/2009/02/irregexp-google-chromes-new-regexp.html)
- Generating machine code from an Abstract Syntax Tree (AST); example: PCRE JIT compiler



## Single Choice Engine with Pre-generated State Machine

## Single Choice Engine with State Tracking
