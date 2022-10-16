/*
 * glibc style testcase:
 * this file should be included from testcases, the testcase should define a function:
 * int do_test(void)
 *
 * The do_test return 0 to indicate a passing test,
 * 1 to indicate a failing test
 * 2 to indicate an unsupported test
 */
extern int do_test(void);

int main(int argc, char **argv)
{
	return do_test();
}
