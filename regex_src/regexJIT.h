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

// The value returned by regex_compile
// Can be used for multiple matching
struct regex_machine;

// A matching state
struct regex_match;

// If error occures the function returns NULL, and the error code returned in error variable
// You can pass NULL to error if you don't care about the error code
struct regex_machine* regex_compile(regex_char_t *regex_string, int length, int *error);
void regex_free_machine(struct regex_machine *machine);

struct regex_match* regex_begin_match(struct regex_machine *machine);
void regex_continue_match(struct regex_match *match, regex_char_t *input_string, int length);
int regex_get_result(struct regex_match *match, int *end, int *id);
// Returns true, if the best match has already found
int regex_is_match_finished(struct regex_match *match);
void regex_free_match(struct regex_match *match);

// Only exists if REGEX_VERBOSE is defined
void regex_continue_match_debug(struct regex_match *match, regex_char_t *input_string, int length, int verbose, int sanity);

#endif
