#include <gtest/gtest.h>

// A simple test
TEST(ExampleTest, BasicAssertions) {
  EXPECT_EQ(1 + 1, 2);
}

// Main function for Google Test
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}