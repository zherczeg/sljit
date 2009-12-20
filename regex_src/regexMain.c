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

#include "regexJIT.h"

#include <stdio.h>

#ifdef REGEX_USE_8BIT_CHARS
#define S(str)	str
#else
#define S(str)	L##str
#endif

void verbose_test(regex_char_t *pattern, regex_char_t *string)
{
	int error;
	regex_char_t *ptr;
	struct regex_machine* machine;
	struct regex_match* match;
	int begin, end, id;

	ptr = pattern;
	while (*ptr)
		ptr++;

	printf("Start test '%s' matches to '%s'\n", pattern, string);
	machine = regex_compile(pattern, ptr - pattern, REGEX_MATCH_VERBOSE, &error);

	if (error) {
		printf("WARNING: Error %d\n", error);
		return;
	}
	if (!machine) {
		printf("ERROR: machine must be exists. Report this bug, please\n");
		return;
	}

	match = regex_begin_match(machine);
	if (!match) {
		printf("WARNING: Not enough memory for matching\n");
		regex_free_machine(machine);
		return;
	}

	ptr = string;
	while (*ptr)
		ptr++;

	regex_continue_match_debug(match, string, ptr - string);

	begin = regex_get_result(match, &end, &id);
	printf("Math returns: %3d->%3d [%3d]\n", begin, end, id);

	regex_free_match(match);
	regex_free_machine(machine);
}

struct test_case {
	int begin;	// awaited begin
	int end;	// awaited end
	int id;		// awaited id
	int finished;	// -1 : don't care, 0 : false, 1 : true
	int flags;	// REGEX_MATCH_*
	regex_char_t *pattern;	// NULL : use the previous pattern
	regex_char_t *string;	// NULL : end of tests
};

void run_tests(struct test_case* test)
{
	int error;
	regex_char_t *ptr;
	struct regex_machine* machine = NULL;
	struct regex_match* match;
	int begin, end, id, finished;
	int success = 0, fail = 0;

	for ( ; test->string ; test++) {
		printf("test: '%s' '%s': ", test->pattern ? test->pattern : "[[REUSE]]", test->string);
		fail++;

		if (test->pattern) {
			if (machine)
				regex_free_machine(machine);

			ptr = test->pattern;
			while (*ptr)
				ptr++;

			machine = regex_compile(test->pattern, ptr - test->pattern, test->flags, &error);

			if (error) {
				printf("ABORT: Error %d\n", error);
				return;
			}
			if (!machine) {
				printf("ABORT: machine must be exists. Report this bug, please\n");
				return;
			}
		}

		ptr = test->string;
		while (*ptr)
			ptr++;

		match = regex_begin_match(machine);
		if (!match) {
			printf("ABORT: Not enough memory for matching\n");
			regex_free_machine(machine);
			return;
		}
		regex_continue_match_debug(match, test->string, ptr - test->string);
		begin = regex_get_result(match, &end, &id);
		finished = regex_is_match_finished(match);

		if (begin != test->begin || end != test->end || id != test->id) {
			printf("FAIL A: begin: %d != %d || end: %d != %d || id: %d != %d\n", test->begin, begin, test->end, end, test->id, id);
			continue;
		}
		if (test->finished != -1 && test->finished != !!finished) {
			printf("FAIL A: finish check\n");
			continue;
		}

		regex_reset_match(match);
		regex_continue_match(match, test->string, ptr - test->string);
		begin = regex_get_result(match, &end, &id);
		finished = regex_is_match_finished(match);
		regex_free_match(match);

		if (begin != test->begin || end != test->end || id != test->id) {
			printf("FAIL B: begin: %d != %d || end: %d != %d || id: %d != %d\n", test->begin, begin, test->end, end, test->id, id);
			continue;
		}
		if (test->finished != -1 && test->finished != !!finished) {
			printf("FAIL B: finish check\n");
			continue;
		}

		printf("SUCCESS\n");
		fail--;
		success++;
	}
	if (machine)
		regex_free_machine(machine);

	if (fail == 0)
		printf("Summary: Success: %d (all)\n", success);
	else
		printf("Summary: Success: %d Fail: %d\n", success, fail);
}

// Testing

static struct test_case tests[] = {
{ 3, 7, 0, -1, 0,
  S("text"), S("is textile") },
{ 0, 10, 0, -1, 0,
  S("^(ab|c)*?d+(es)?"), S("abccabddeses") },
{ -1, 0, 0, 1, 0,
  S("^a+"), S("saaaa") },
{ 3, 6, 0, 0, 0,
  S("(a+|b+)$"), S("saabbb") },
{ 1, 6, 0, 0, 0,
  S("(a+|b+){,2}$"), S("saabbb") },
{ 1, 6, 0, 1, 0,
  S("(abcde|bc)(a+*|(b|c){2}+){0}"), S("babcdeaaaaaaaa") },
{ 1, 6, 0, 1, 0,
  S("(abc(aa)?|(cab+){2})"), S("cabcaa") },
{ -1, 0, 0, 1, 0,
  S("^(abc(aa)?|(cab+){2})$"), S("cabcaa") },
{ 0, 3, 1, -1, 0,
  S("^(ab{001!})?c"), S("abcde") },
{ 1, 15, 2, -1, 0,
  S("(c?(a|bb{2!}){2,3}()+d){2,3}"), S("ccabbadbbadcaadcaad") },
{ 2, 9, 0, -1, 0,
  NULL, S("cacaadaadaa") },
{ -1, 0, 0, -1, REGEX_MATCH_BEGIN,
  S("(((ab?c|d{1})))"), S("ad") },
{ 0, 9, 3, -1, REGEX_MATCH_BEGIN,
  S("^((a{1!}|b{2!}|c{3!}){3,6}d)+"), S("cabadbacddaa") },
{ 1, 6, 0, 0, REGEX_MATCH_END,
  S("(a+(bb|cc?)?){4,}"), S("maaaac") },
{ 3, 12, 1, 0, REGEX_MATCH_END,
  S("(x+x+{02,03}(x+|{1!})){03,06}$"), S("aaaxxxxxxxxx") },
{ 1, 2, 3, -1, 0,
  S("((c{1!})?|x+{2!}|{3!})(a|c)"), S("scs") },
{ 1, 4, 2, 1, 0,
  NULL, S("sxxaxxxaccacca") },
{ 0, 2, 1, 1, 0,
  NULL, S("ccdcdcdddddcdccccd") },
{ 0, 3, 0, -1, REGEX_MATCH_NON_GREEDY,
  S("^a+a+a+"), S("aaaaaa") },
{ 2, 5, 0, -1, REGEX_MATCH_NON_GREEDY,
  S("a+a+a+"), S("bbaaaaaa") },
{ 1, 4, 0, 1, 0,
  S("baa|a+"), S("sbaaaaaa") },
{ 0, 6, 0, 1, 0,
  S("baaa|baa|sbaaaa"), S("sbaaaaa") },
{ 1, 4, 0, 1, REGEX_MATCH_NON_GREEDY,
  S("baaa|baa"), S("xbaaa") },
{ 4, 12, 0, 1, 0,
  S("(.[]-]){3}[^]-]{2}"), S("ax-xs-[][]lmn") },
{ 3, 7, 1, 1, 0,
  S("([ABC]|[abc]{1!}){3,5}"), S("AbSAabbx") },
{ 0, 8, 3, 0, 0,
  S("^[x\\-y[\\]]+([[\\]]{3!})*$"), S("x-y[-][]") },
{ 0, 9, 0, 0, 0,
  NULL, S("x-y[-][]x") },
{ 2, 8, 0, 1, 0,
  S("<(/{1!})?[^>]+>"), S("  <html></html> ") },
{ 2, 9, 1, 1, 0,
  NULL, S("  </html><html> ") },
{ 2, 9, 0, 1, 0,
  S("[A-Z0-9a-z]+"), S("[(Iden9aA)]") },
{ 1, 4, 0, 1, 0,
  S("[^x-y]+[a-c_]{2,3}"), S("x_a_y") },
{ 4, 11, 0, 0, 0,
  NULL, S("ssaymmaa_ccl") },
{ -1, 0, 0, 0, 0,
  NULL, NULL }
};

int main(int argc, char* argv[])
{
//	verbose_test("a((b)((c|d))|)c|");
//	verbose_test("Xa{009,0010}Xb{,7}Xc{5,}Xd{,}Xe{1,}Xf{,1}X");
//	verbose_test("{3!}({3})({0!}){,");
//	verbose_test("(s(ab){2,4}t){2,}*S(a*(b)(c()|)d+){3,4}{0,0}*M");
//	verbose_test("^a({2!})*b+(a|{1!}b)+d$");
//	verbose_test("((a|b|c)*(xy)+)+", "asbcxyxy");

	run_tests(tests);
	return 0;
}

