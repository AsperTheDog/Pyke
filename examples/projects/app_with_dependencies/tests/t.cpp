#include <gtest/gtest.h>
#include <hdr/clamp.hpp>
TEST(Clamp, Works){ EXPECT_EQ(hdr::clamp(5,0,3),3); }
