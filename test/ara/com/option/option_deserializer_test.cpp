#include <gtest/gtest.h>
#include <stdexcept>
#include "../../../../src/ara/com/option/option_deserializer.h"

namespace ara
{
    namespace com
    {
        namespace option
        {
            static void expectTruncatedPayloadsThrow(const std::vector<uint8_t> &payload)
            {
                for (std::size_t _length = 0; _length < payload.size(); ++_length)
                {
                    const std::vector<uint8_t> cTruncatedPayload(
                        payload.begin(), payload.begin() + _length);
                    std::size_t _offset{0};

                    SCOPED_TRACE("Truncated length = " + std::to_string(_length));
                    EXPECT_THROW(
                        OptionDeserializer::Deserialize(cTruncatedPayload, _offset),
                        std::out_of_range);
                }
            }

            TEST(OptionDeserializerTest, EmptyPayload)
            {
                const std::vector<uint8_t> cPayload;
                std::size_t _offset{0};

                EXPECT_THROW(
                    OptionDeserializer::Deserialize(cPayload, _offset),
                    std::out_of_range);
            }

            TEST(OptionDeserializerTest, TruncatedIpv4EndpointOption)
            {
                std::unique_ptr<Ipv4EndpointOption> _option{
                    Ipv4EndpointOption::CreateUnitcastEndpoint(
                        true, helper::Ipv4Address(127, 0, 0, 1), Layer4ProtocolType::Tcp, 8080)};
                const std::vector<uint8_t> cPayload{_option->Payload()};

                expectTruncatedPayloadsThrow(cPayload);
            }

            TEST(OptionDeserializerTest, TruncatedLoadBalancingOption)
            {
                LoadBalancingOption _option(false, 1, 2);
                const std::vector<uint8_t> cPayload{_option.Payload()};

                expectTruncatedPayloadsThrow(cPayload);
            }

            TEST(OptionDeserializerTest, OffsetBeyondPayload)
            {
                LoadBalancingOption _option(false, 1, 2);
                const std::vector<uint8_t> cPayload{_option.Payload()};
                std::size_t _offset{cPayload.size()};

                EXPECT_THROW(
                    OptionDeserializer::Deserialize(cPayload, _offset),
                    std::out_of_range);
            }

            TEST(OptionDeserializerTest, UnsupportedOptionType)
            {
                const std::size_t cTypeIndex{2};
                LoadBalancingOption _option(false, 1, 2);
                std::vector<uint8_t> _payload{_option.Payload()};
                _payload.at(cTypeIndex) = static_cast<uint8_t>(OptionType::Configuration);
                std::size_t _offset{0};

                EXPECT_THROW(
                    OptionDeserializer::Deserialize(_payload, _offset),
                    std::out_of_range);
            }

            TEST(OptionDeserializerTest, CompletePayloadAtOffset)
            {
                const std::vector<uint8_t> cPrefix{0xff, 0xff};
                const uint16_t cPriority{1};
                const uint16_t cWeight{2};
                LoadBalancingOption _originalOption(true, cPriority, cWeight);
                const std::vector<uint8_t> cOptionPayload{_originalOption.Payload()};

                std::vector<uint8_t> _payload{cPrefix};
                _payload.insert(_payload.end(), cOptionPayload.begin(), cOptionPayload.end());
                std::size_t _offset{cPrefix.size()};

                std::unique_ptr<Option> _deserializedOptionBase{
                    OptionDeserializer::Deserialize(_payload, _offset)};
                LoadBalancingOption *_deserializedOption{
                    dynamic_cast<LoadBalancingOption *>(_deserializedOptionBase.get())};

                ASSERT_NE(_deserializedOption, nullptr);
                EXPECT_EQ(_deserializedOption->Discardable(), _originalOption.Discardable());
                EXPECT_EQ(_deserializedOption->Priority(), cPriority);
                EXPECT_EQ(_deserializedOption->Weight(), cWeight);
                EXPECT_EQ(_offset, _payload.size());
            }
        }
    }
}
