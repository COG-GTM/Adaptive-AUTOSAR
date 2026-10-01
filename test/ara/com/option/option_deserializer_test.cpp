#include <gtest/gtest.h>
#include "../../../../src/ara/com/option/option_deserializer.h"

namespace ara
{
    namespace com
    {
        namespace option
        {
            TEST(OptionDeserializerTest, UnicastEndpointOption)
            {
                const bool cDiscardable{false};
                const helper::Ipv4Address cIpAddress(192, 168, 1, 2);
                const Layer4ProtocolType cProtocol{Layer4ProtocolType::Udp};
                const uint16_t cPort{30501};

                auto _originalOption{
                    Ipv4EndpointOption::CreateUnitcastEndpoint(
                        cDiscardable, cIpAddress, cProtocol, cPort)};
                const std::vector<uint8_t> cPayload{_originalOption->Payload()};

                std::size_t _offset{0};
                auto _deserializedOption{OptionDeserializer::Deserialize(cPayload, _offset)};

                auto _endpointOption{
                    dynamic_cast<Ipv4EndpointOption *>(_deserializedOption.get())};
                ASSERT_NE(nullptr, _endpointOption);

                EXPECT_EQ(OptionType::IPv4Endpoint, _endpointOption->Type());
                EXPECT_EQ(cDiscardable, _endpointOption->Discardable());
                EXPECT_EQ(cIpAddress, _endpointOption->IpAddress());
                EXPECT_EQ(cProtocol, _endpointOption->L4Proto());
                EXPECT_EQ(cPort, _endpointOption->Port());
                EXPECT_EQ(cPayload.size(), _offset);
            }

            TEST(OptionDeserializerTest, MulticastEndpointOption)
            {
                const bool cDiscardable{true};
                const helper::Ipv4Address cIpAddress(239, 0, 0, 1);
                const uint16_t cPort{30502};

                auto _originalOption{
                    Ipv4EndpointOption::CreateMulticastEndpoint(
                        cDiscardable, cIpAddress, cPort)};
                const std::vector<uint8_t> cPayload{_originalOption->Payload()};

                std::size_t _offset{0};
                auto _deserializedOption{OptionDeserializer::Deserialize(cPayload, _offset)};

                auto _endpointOption{
                    dynamic_cast<Ipv4EndpointOption *>(_deserializedOption.get())};
                ASSERT_NE(nullptr, _endpointOption);

                EXPECT_EQ(OptionType::IPv4Multicast, _endpointOption->Type());
                EXPECT_EQ(cDiscardable, _endpointOption->Discardable());
                EXPECT_EQ(cIpAddress, _endpointOption->IpAddress());
                EXPECT_EQ(cPort, _endpointOption->Port());
            }

            TEST(OptionDeserializerTest, SdEndpointOption)
            {
                const helper::Ipv4Address cIpAddress(10, 0, 0, 1);
                const Layer4ProtocolType cProtocol{Layer4ProtocolType::Udp};
                const uint16_t cPort{30490};

                auto _originalOption{
                    Ipv4EndpointOption::CreateSdEndpoint(false, cIpAddress, cProtocol, cPort)};
                const std::vector<uint8_t> cPayload{_originalOption->Payload()};

                std::size_t _offset{0};
                auto _deserializedOption{OptionDeserializer::Deserialize(cPayload, _offset)};

                auto _endpointOption{
                    dynamic_cast<Ipv4EndpointOption *>(_deserializedOption.get())};
                ASSERT_NE(nullptr, _endpointOption);

                EXPECT_EQ(OptionType::IPv4SdEndpoint, _endpointOption->Type());
                EXPECT_EQ(cIpAddress, _endpointOption->IpAddress());
                EXPECT_EQ(cProtocol, _endpointOption->L4Proto());
                EXPECT_EQ(cPort, _endpointOption->Port());
            }

            TEST(OptionDeserializerTest, LoadBalancingOptionType)
            {
                const bool cDiscardable{true};
                const uint16_t cPriority{0x0102};
                const uint16_t cWeight{0x0304};

                const LoadBalancingOption cOriginalOption(cDiscardable, cPriority, cWeight);
                const std::vector<uint8_t> cPayload{cOriginalOption.Payload()};

                std::size_t _offset{0};
                auto _deserializedOption{OptionDeserializer::Deserialize(cPayload, _offset)};

                auto _loadBalancingOption{
                    dynamic_cast<LoadBalancingOption *>(_deserializedOption.get())};
                ASSERT_NE(nullptr, _loadBalancingOption);

                EXPECT_EQ(OptionType::LoadBalancing, _loadBalancingOption->Type());
                EXPECT_EQ(cDiscardable, _loadBalancingOption->Discardable());
                EXPECT_EQ(cPriority, _loadBalancingOption->Priority());
                EXPECT_EQ(cWeight, _loadBalancingOption->Weight());
                EXPECT_EQ(cPayload.size(), _offset);
            }

            TEST(OptionDeserializerTest, ConsecutiveOptions)
            {
                const helper::Ipv4Address cIpAddress(127, 0, 0, 1);
                const uint16_t cPort{30503};
                const uint16_t cPriority{7};
                const uint16_t cWeight{9};

                auto _endpointOption{
                    Ipv4EndpointOption::CreateUnitcastEndpoint(
                        false, cIpAddress, Layer4ProtocolType::Tcp, cPort)};
                const LoadBalancingOption cLoadBalancingOption(false, cPriority, cWeight);

                std::vector<uint8_t> _payload{_endpointOption->Payload()};
                const std::vector<uint8_t> cSecondPayload{cLoadBalancingOption.Payload()};
                _payload.insert(_payload.end(), cSecondPayload.cbegin(), cSecondPayload.cend());

                std::size_t _offset{0};
                auto _firstOption{OptionDeserializer::Deserialize(_payload, _offset)};
                auto _secondOption{OptionDeserializer::Deserialize(_payload, _offset)};

                auto _firstEndpoint{dynamic_cast<Ipv4EndpointOption *>(_firstOption.get())};
                ASSERT_NE(nullptr, _firstEndpoint);
                EXPECT_EQ(cPort, _firstEndpoint->Port());

                auto _secondLoadBalancing{
                    dynamic_cast<LoadBalancingOption *>(_secondOption.get())};
                ASSERT_NE(nullptr, _secondLoadBalancing);
                EXPECT_EQ(cPriority, _secondLoadBalancing->Priority());
                EXPECT_EQ(cWeight, _secondLoadBalancing->Weight());

                EXPECT_EQ(_payload.size(), _offset);
            }

            TEST(OptionDeserializerTest, UnsupportedOptionType)
            {
                const std::vector<uint8_t> cPayload{
                    0x00, 0x09,
                    static_cast<uint8_t>(OptionType::IPv6Endpoint),
                    0x00};

                std::size_t _offset{0};

                EXPECT_THROW(
                    OptionDeserializer::Deserialize(cPayload, _offset),
                    std::out_of_range);
            }

            TEST(OptionDeserializerTest, TruncatedPayload)
            {
                const std::vector<uint8_t> cPayload{0x00, 0x05};
                std::size_t _offset{0};

                EXPECT_THROW(
                    OptionDeserializer::Deserialize(cPayload, _offset),
                    std::out_of_range);
            }
        }
    }
}
