#include "unity.h"
#include <stdio.h>

void setUp(void) {}

void tearDown(void) {}

void test_pass(void) { TEST_ASSERT_TRUE(1); }

void test_fail(void) { TEST_ASSERT_FALSE(0); }

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_pass);
  RUN_TEST(test_fail);
  return UNITY_END();
}
