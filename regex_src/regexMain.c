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

void test_case(regex_char_t *pattern, regex_char_t *string)
{
	int error;
	regex_char_t *ptr;
	struct regex_machine* machine;
	struct regex_match* match;

	ptr = pattern;
	while (*ptr)
		ptr++;

	printf("Start test '%s'\n", pattern);
	machine = regex_compile(pattern, ptr - pattern, &error);

	if (error) {
		printf("WARNING: Error %d\n", error);
		return;
	}
	if (!machine) {
		printf("ERROR: machine must be exists. Reportthis bug please\n");
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

	regex_continue_match_debug(match, string, ptr - string, 1, 1);

	regex_free_match(match);
	regex_free_machine(machine);
}

int main(int argc, char* argv[])
{

//	test_case("Xa*X.+Xc?X()*Xa+*?X");
//	test_case("a((b)((c|d))|)c|");
//	test_case("Xa{009,0010}Xb{,7}Xc{5,}Xd{,}Xe{1,}Xf{,1}X");
//	test_case("Xa:2:*X|Yb:03:+Y");
//	test_case("{3!}({3})({0!}){,");
//	test_case("(s(ab){2,4}t){2,}*S(a*(b)(c()|)d+){3,4}{0,0}*M");
//	test_case("^a({2!})*b+(a|{1!}b)+d$");
	test_case("(aa|{1!}bb)*", "aabbaa");
	test_case("(aa|bb)*", "aabbaa");
	test_case("^(aa|bb)*", "aabbaa");
	return 0;
}

