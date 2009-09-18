/*
 *    Stack-less Just-In-Time compiler
 *    Copyright (c) Zoltan Herczeg
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as
 *   published by the Free Software Foundation.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 */

#ifndef _REGEX_JIT_H_
#define _REGEX_JIT_H_

// Character type config
#define REGEX_USE_8BIT_CHARS

#ifdef REGEX_USE_8BIT_CHARS
typedef char regex_char_t;
#else
typedef wchar_t regex_char_t;
#endif

// Error codes
#define REGEX_NO_ERROR		0
#define REGEX_MEMORY_ERROR	1
#define REGEX_INVALID_REGEX	2

// Note: large, nested {a,b} iterations can blow up the memory consumption
// a{n,m} is replaced by aa...aaa?a?a?a?a? (n >= 0, m > 0)
//                       \__n__/\____m___/
// a{n,}  is replaced by aa...aaa+ (n > 0)
//                       \_n-1_/

// If error occures the function returns NULL, and the error code returned in error variable
// You can pass NULL to error if you don't care about the error code
void* regex_compile(regex_char_t *regex_string, int length, int *error);

#endif
