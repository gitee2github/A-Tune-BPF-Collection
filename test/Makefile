.DEFAULT_GOAL := check
INCLUDES := -I../

evaluate-test := evaluate-test.sh
# test source code functions in object files
test_objs := tst-common_helper

# Build C testcases code
$(patsubst %, %.o, $(test_objs)): %.o: %.c
	$(Q)$(CC) $(CFLAGS) $(INCLUDES) -c $(filter %.c,$^) -o $@

$(test_objs): %: %.o
	$(Q)$(CC) $(CFLAGS) $^ ../$(OUTPUT)/$(strip $(subst tst-, ,$@)).o -o $@

# all the lines in the recipe be passed to a single invocation of the shell
.ONESHELL:
SHELL = /bin/sh

define run_tests
	unsupport=0
	fail=0
	pass=0
	for testcase in $1; do
		./$$testcase >$$testcase.out 2>&1 
		ret=$$?
		$(SHELL) $(evaluate-test) $$testcase $$ret
		if [ $$ret -eq 2 ]; then
			unsupport=`expr $$unsupport + 1`
		else
			if [ $$ret -eq 0 ]; then
				pass=`expr $$pass + 1`
			else
				fail=`expr $$fail + 1`
			fi
		fi
	done

	echo -e "    Test Summary:"
	echo -e "      Unsupport: $$unsupport"
	echo -e "      Fail: $$fail"
	echo -e "      Pass: $$pass"
endef

.PHONY: check
check: $(test_objs)
	@$(call run_tests, $<)

.PHONY: clean
clean:
	$(Q)rm -rf $(test_objs) $(patsubst %,%.o,$(test_objs)) $(patsubst %,%.out,$(test_objs))
