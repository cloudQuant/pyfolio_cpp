add_test([=[ImguiTest.TestFirst]=]  /Users/yunjinqi/Documents/pyfolio_cpp/build/pyfolio_test [==[--gtest_filter=ImguiTest.TestFirst]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[ImguiTest.TestFirst]=]  PROPERTIES WORKING_DIRECTORY /Users/yunjinqi/Documents/pyfolio_cpp/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
set(  pyfolio_test_TESTS ImguiTest.TestFirst)
