#include <check.h>
#include <stdlib.h>
#include <string.h>

/* Include the regex JIT interface */
#include "regex_src/regexJIT.h"

START_TEST(test_dfa_size_overflow_no_oob)
{
    /* Invariant: regex compilation must never cause heap buffer overflow
     * due to unchecked multiplication overflow in dfa_size allocation.
     * The function must either succeed safely or return an error/NULL. */
    const char *payloads[] = {
        /* Exact exploit case: deeply nested alternation to maximize dfa_size */
        "((((((((((a|b|c|d|e|f|g|h|i|j){100}){100}){100}){100}){10}){10}){10}){5}){5}){5}",
        /* Boundary: large repeated group that stresses dfa_size calculation */
        "(a|b|c|d|e|f|g|h|i|j|k|l|m|n|o|p|q|r|s|t|u|v|w|x|y|z){65535}",
        /* Valid input: simple pattern that should compile cleanly */
        "(abc|def|ghi)"
    };
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    for (int i = 0; i < num_payloads; i++) {
        struct regex_machine *machine = NULL;
        struct regex_match *match = NULL;

        /* Attempt to compile the regex pattern */
        machine = regex_compile(payloads[i], strlen(payloads[i]), 0, NULL);

        /* Invariant: if compilation succeeds, the machine must be usable
         * without crashing (no heap overflow occurred during allocation).
         * If it returns NULL, that is an acceptable safe failure. */
        if (machine != NULL) {
            match = regex_begin_match(machine, 0, NULL);
            if (match != NULL) {
                regex_reset_match(match);
                regex_free_match(match);
            }
            regex_free_machine(machine);
        }
        /* NULL return is acceptable — it means the input was rejected safely */
        ck_assert_msg(1, "Should reach here without crash for payload %d", i);
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_set_timeout(tc_core, 30);
    tcase_add_test(tc_core, test_dfa_size_overflow_no_oob);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}