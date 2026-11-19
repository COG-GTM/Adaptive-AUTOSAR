#include <cstdint>
#include <type_traits>
#include <gtest/gtest.h>
#include "../../../src/ara/sm/states.h"

namespace ara
{
    namespace sm
    {
        TEST(SMStatesTest, FunctionGroupStatesUnderlyingValues)
        {
            const bool cIsUint8 =
                std::is_same<
                    std::underlying_type<FunctionGroupStates>::type,
                    uint8_t>::value;
            EXPECT_TRUE(cIsUint8);
            EXPECT_EQ(0, static_cast<uint8_t>(FunctionGroupStates::kOff));
            EXPECT_EQ(1, static_cast<uint8_t>(FunctionGroupStates::kRunning));
            EXPECT_EQ(2, static_cast<uint8_t>(FunctionGroupStates::kUpdate));
            EXPECT_EQ(3, static_cast<uint8_t>(FunctionGroupStates::kVerify));
        }
    }
}
