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

int main(int argc, char* argv[])
{
	int error;

//	regex_compile("Xa*X.+Xc?X()*Xa+*?X", 19, &error);
//	regex_compile("a((b)((c|d))|)c|", 16, &error);
//	regex_compile("Xa{0910,0990}Xb{,7}Xc{5,}Xd{,}Xe{1,}Xf{,1}X", 43, &error);
//	regex_compile("Xa:2:*X|Yb:03:+Y", 16, &error);
	regex_compile("A(Xb{2,}*Ys+){0,3}*By*", 22, &error);
//	regex_compile("(s(ab){2,4}t){2,}*S(a*(b)(c()|)d+){3,4}{0,0}*M", 46, &error);

	printf("error %d\n", error);
	return 0;
}

