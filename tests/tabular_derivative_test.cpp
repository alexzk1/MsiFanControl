#include "tabular_derivative.h"

#include <chrono>
#include <thread>

#include <gtest/gtest.h>

/// @brief class TabularDerivative tests.
namespace Test {

using namespace std::chrono_literals;

class TabularDerivativeTest : public ::testing::Test
{
  public:
};

TEST_F(TabularDerivativeTest, ItWorksNoSmoothing)
{
    TabularDerivative derivative(1.0f);
    const auto add_value = [&derivative](float value) {
        derivative.Update(value);
        std::this_thread::sleep_for(1ms);
    };
    add_value(1.0);
    ASSERT_FALSE(derivative.Result());
    add_value(2.0);
    ASSERT_TRUE(derivative.Result());
    EXPECT_GT(*derivative.Result(), 0.0f);
    add_value(3.0);
    ASSERT_TRUE(derivative.Result());
    EXPECT_GT(*derivative.Result(), 0.0f);
    add_value(3.0);
    add_value(3.0);
    add_value(3.0);
    add_value(3.0);
    add_value(3.0);
    add_value(3.0);
    ASSERT_TRUE(derivative.Result());
    EXPECT_NEAR(*derivative.Result(), 0.0f, 0.001f);
    add_value(2.0);
    add_value(1.0);
    add_value(0.0);
    ASSERT_TRUE(derivative.Result());
    EXPECT_LT(*derivative.Result(), 0.0f);
}

} // namespace Test
