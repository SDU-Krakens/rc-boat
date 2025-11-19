#include "unity.h"

void setUp(void) {}

void tearDown(void) {}

void test_pass(void) { TEST_ASSERT_TRUE(1); }

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_pass);
  return UNITY_END();
}
