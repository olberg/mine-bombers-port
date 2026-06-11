#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

void test_sanity(void)
{
    TEST_ASSERT_EQUAL(42, 42);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_sanity);
    return UNITY_END();
}
