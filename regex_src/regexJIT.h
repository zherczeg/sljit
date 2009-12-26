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

// Note: REGEX_MATCH_BEGIN and REGEX_MATCH_END does not change the parsing
//   (Hence ^ and $ are parsed normally)
// Force matching to start from begining of the string (same as ^)
#define REGEX_MATCH_BEGIN	0x01
// Force matching to continue until the last character (same as $)
#define REGEX_MATCH_END		0x02
// Changes . to [^\r\n] and appends \r\n to all [^...] expressions
#define REGEX_NEWLINE		0x04
// Non greedy matching. In case of Thompson (non-recursive) algorithm,
// it (usually) does not have a significant speed gain
#define REGEX_MATCH_NON_GREEDY	0x08
// Verbose. This define can be commented out, which disables all verbose features
#define REGEX_MATCH_VERBOSE	0x10

// If error occures the function returns NULL, and the error code returned in error variable
// You can pass NULL to error if you don't care about the error code
// The re_flags argument contains the default REGEX_MATCH flags. See above
struct regex_machine* regex_compile(regex_char_t *regex_string, int length, int re_flags, int *error);
void regex_free_machine(struct regex_machine *machine);

// Create and init match structure for a given machine
struct regex_match* regex_begin_match(struct regex_machine *machine);
void regex_reset_match(struct regex_match *match);
void regex_free_match(struct regex_match *match);

// Pattern matching
// regex_continue_match does not support REGEX_MATCH_VERBOSE flag
void regex_continue_match(struct regex_match *match, regex_char_t *input_string, int length);
int regex_get_result(struct regex_match *match, int *end, int *id);
// Returns true, if the best match has already found
int regex_is_match_finished(struct regex_match *match);

// Only exists if VERBOSE is defined in regexJIT.c
// Do both sanity check and verbose. (The latter only if REGEX_MATCH_VERBOSE was passed to regex_compile)
void regex_continue_match_debug(struct regex_match *match, regex_char_t *input_string, int length);

#endif
