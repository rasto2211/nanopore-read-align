#include <iostream>

#include "log2_num.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

TEST(Log2Num, Log2NumProductTest) {
  Log2Num prod = Log2Num(0.5) * Log2Num(0.4);
  EXPECT_DOUBLE_EQ(Log2Num(0.5 * 0.4).value(), prod.value());
}

// Normal multiplication of doubles 10^-300 * 10^-300 * 10^300 would overflow.
// In this test it shouldn't.
TEST(Log2Num, Log2NumProductSmallNumberTest) {
  Log2Num prod = Log2Num(1.0e-300) * Log2Num(1.0e-300) * Log2Num(1.0e300);
  EXPECT_DOUBLE_EQ(Log2Num(1.0e-300).value(), prod.value());
}

// Test for operator *=.
TEST(Log2Num, Log2NumTimesEqualSignTest) {
  Log2Num prod = Log2Num(1.0e-300);
  prod *= Log2Num(1.0e-300);
  prod *= Log2Num(1.0e300);
  EXPECT_DOUBLE_EQ(Log2Num(1.0e-300).value(), prod.value());
}

// Test with number 0. Log(0) is undefined.
TEST(Log2Num, Log2NumZeroProdTest) {
  Log2Num prod = Log2Num(1.0e-300);
  prod *= Log2Num(0);
  EXPECT_DOUBLE_EQ(Log2Num(0).value(), prod.value());
}

TEST(Log2Num, Log2NumSumTest) {
  Log2Num sum = Log2Num(0.5) + Log2Num(0.1);
  EXPECT_DOUBLE_EQ(Log2Num(0.5 + 0.1).value(), sum.value());
}
