#include <gtest/gtest.h>
#include "../../../../src/ara/com/e2e/profile11.h"

namespace ara
{
    namespace com
    {
        namespace e2e
        {
            TEST(Profile11CounterTest, ForwardNotImplemented)
            {
                Profile11 _profile;
                const std::vector<uint8_t> cUnprotectedData{0x12, 0x34, 0x56, 0x78};
                std::vector<uint8_t> _protectedData;

                EXPECT_THROW(
                    _profile.TryForward(cUnprotectedData, _protectedData),
                    std::logic_error);
                EXPECT_TRUE(_protectedData.empty());
            }

            TEST(Profile11CounterTest, ProtectedDataLayout)
            {
                const size_t cCounterOffset{1};
                const size_t cHeaderSize{2};
                const uint8_t cExpectedCounter{0xf1};
                Profile11 _profile;
                const std::vector<uint8_t> cUnprotectedData{0x12, 0x34, 0x56, 0x78};
                std::vector<uint8_t> _protectedData;

                ASSERT_TRUE(_profile.TryProtect(cUnprotectedData, _protectedData));

                ASSERT_EQ(cUnprotectedData.size() + cHeaderSize, _protectedData.size());
                EXPECT_EQ(cExpectedCounter, _protectedData.at(cCounterOffset));
                const std::vector<uint8_t> cActualData(
                    _protectedData.cbegin() + cHeaderSize, _protectedData.cend());
                EXPECT_EQ(cUnprotectedData, cActualData);
            }

            TEST(Profile11CounterTest, CounterWrapAroundScenario)
            {
                const size_t cCounterOffset{1};
                const uint8_t cCounterMask{0x0f};
                const uint8_t cCounterMax{0x0e};
                const int cIterations{32};
                Profile11 _sender;
                Profile11 _receiver;
                const std::vector<uint8_t> cUnprotectedData{0x12, 0x34, 0x56, 0x78};
                uint8_t _expectedCounter{0};

                for (int i = 0; i < cIterations; ++i)
                {
                    std::vector<uint8_t> _protectedData;
                    ASSERT_TRUE(_sender.TryProtect(cUnprotectedData, _protectedData));

                    _expectedCounter =
                        (_expectedCounter < cCounterMax) ? _expectedCounter + 1 : 0;
                    const uint8_t cActualCounter{
                        static_cast<uint8_t>(
                            _protectedData.at(cCounterOffset) & cCounterMask)};
                    EXPECT_EQ(_expectedCounter, cActualCounter);

                    // The receiver should accept the wrapped counter as a valid sequence.
                    EXPECT_EQ(CheckStatusType::kOk, _receiver.Check(_protectedData));
                }
            }

            TEST(Profile11CounterTest, RepeatedThenWrongSequenceScenario)
            {
                Profile11 _sender;
                Profile11 _receiver;
                const std::vector<uint8_t> cUnprotectedData{0xaa, 0xbb};
                std::vector<uint8_t> _firstProtectedData;
                std::vector<uint8_t> _secondProtectedData;

                ASSERT_TRUE(_sender.TryProtect(cUnprotectedData, _firstProtectedData));
                ASSERT_TRUE(_sender.TryProtect(cUnprotectedData, _secondProtectedData));

                EXPECT_EQ(CheckStatusType::kOk, _receiver.Check(_firstProtectedData));
                EXPECT_EQ(CheckStatusType::kOk, _receiver.Check(_secondProtectedData));
                EXPECT_EQ(CheckStatusType::kRepeated, _receiver.Check(_secondProtectedData));
                EXPECT_EQ(CheckStatusType::kWrongSequence, _receiver.Check(_firstProtectedData));
            }

            TEST(Profile11CounterTest, CorruptedPayloadCheck)
            {
                Profile11 _sender;
                Profile11 _receiver;
                const std::vector<uint8_t> cUnprotectedData{0xaa, 0xbb};
                std::vector<uint8_t> _protectedData;

                ASSERT_TRUE(_sender.TryProtect(cUnprotectedData, _protectedData));
                _protectedData.back() ^= 0x01;

                EXPECT_EQ(CheckStatusType::kWrongCrc, _receiver.Check(_protectedData));
            }
        }
    }
}
