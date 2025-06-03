#ifndef RMQTESTUTIL_TESTSUITE_T_H
#define RMQTESTUTIL_TESTSUITE_T_H

#include <gtest/gtest.h>

#if defined(__sun)
// We need to stick to INSTANTIATE_TEST_CASE_P for a while longer
// But we do want to build with -Werror in our CI
#pragma GCC diagnostic warning "-Wdeprecated-declarations"

#define RMQTESTUTIL_TESTSUITE_P INSTANTIATE_TEST_CASE_P
#else
#define RMQTESTUTIL_TESTSUITE_P INSTANTIATE_TEST_SUITE_P
#endif
#endif // RMQTESTUTIL_TESTSUITE_T_H