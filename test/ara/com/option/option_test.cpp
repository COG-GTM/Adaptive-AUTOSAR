#include <gtest/gtest.h>
#include "../../../../src/ara/com/option/ipv4_endpoint_option.h"
#include "../../../../src/ara/com/option/loadbalancing_option.h"

namespace ara
{
    namespace com
    {
        namespace option
        {
            TEST(OptionTest, MulticastEndpointOutOfRangeAddress)
            {
                const helper::Ipv4Address cBelowRange(223, 255, 255, 255);
                const helper::Ipv4Address cAboveRange(240, 0, 0, 0);
                const uint16_t cPort{30490};

                EXPECT_THROW(
                    Ipv4EndpointOption::CreateMulticastEndpoint(
                        false, cBelowRange, cPort),
                    std::invalid_argument);
                EXPECT_THROW(
                    Ipv4EndpointOption::CreateMulticastEndpoint(
                        false, cAboveRange, cPort),
                    std::invalid_argument);
            }

            TEST(OptionTest, MulticastEndpointRangeBoundaries)
            {
                const helper::Ipv4Address cLowerBound(224, 0, 0, 0);
                const helper::Ipv4Address cUpperBound(239, 255, 255, 255);
                const uint16_t cPort{30490};

                EXPECT_NO_THROW(
                    Ipv4EndpointOption::CreateMulticastEndpoint(
                        false, cLowerBound, cPort));
                EXPECT_NO_THROW(
                    Ipv4EndpointOption::CreateMulticastEndpoint(
                        false, cUpperBound, cPort));
            }

            TEST(OptionTest, UnicastEndpointPayload)
            {
                const helper::Ipv4Address cIpAddress(192, 168, 0, 1);
                const uint16_t cPort{8080};
                const std::vector<uint8_t> cExpectedPayload{
                    0x00, 0x09, 0x04, 0x01,
                    0xc0, 0xa8, 0x00, 0x01,
                    0x00, 0x06, 0x1f, 0x90};

                std::unique_ptr<Ipv4EndpointOption> _option{
                    Ipv4EndpointOption::CreateUnitcastEndpoint(
                        true, cIpAddress, Layer4ProtocolType::Tcp, cPort)};

                EXPECT_TRUE(_option->Discardable());
                EXPECT_EQ(cExpectedPayload, _option->Payload());
            }

            TEST(OptionTest, LoadBalancingPayload)
            {
                const std::vector<uint8_t> cExpectedPayload{
                    0x00, 0x05, 0x02, 0x00,
                    0x00, 0x03, 0x01, 0x00};

                const LoadBalancingOption cOption(false, 0x0003, 0x0100);

                EXPECT_FALSE(cOption.Discardable());
                EXPECT_EQ(cExpectedPayload, cOption.Payload());
            }

            TEST(OptionTest, SdEndpointDeserialization)
            {
                const std::vector<uint8_t> cPayload{
                    0x0a, 0x00, 0x00, 0x02,
                    0x00, 0x11, 0x77, 0x1a};
                const helper::Ipv4Address cExpectedIpAddress(10, 0, 0, 2);
                const std::size_t cExpectedOffset{8};
                std::size_t _offset{0};

                std::unique_ptr<Ipv4EndpointOption> _option{
                    Ipv4EndpointOption::Deserialize(
                        cPayload, _offset, OptionType::IPv4SdEndpoint, true)};

                EXPECT_EQ(cExpectedOffset, _offset);
                EXPECT_EQ(OptionType::IPv4SdEndpoint, _option->Type());
                EXPECT_TRUE(_option->Discardable());
                EXPECT_EQ(cExpectedIpAddress, _option->IpAddress());
                EXPECT_EQ(Layer4ProtocolType::Udp, _option->L4Proto());
                EXPECT_EQ(30490, _option->Port());
            }

            TEST(OptionTest, MulticastEndpointDeserializationOutOfRangeAddress)
            {
                // 192.168.0.1 is not a multicast IP address.
                const std::vector<uint8_t> cPayload{
                    0xc0, 0xa8, 0x00, 0x01,
                    0x00, 0x11, 0x77, 0x1a};
                std::size_t _offset{0};

                EXPECT_THROW(
                    Ipv4EndpointOption::Deserialize(
                        cPayload, _offset, OptionType::IPv4Multicast, false),
                    std::invalid_argument);
            }

            TEST(OptionTest, EndpointDeserializationForeignOptionType)
            {
                const std::vector<uint8_t> cPayload{
                    0xc0, 0xa8, 0x00, 0x01,
                    0x00, 0x06, 0x1f, 0x90};
                std::size_t _offset{0};

                EXPECT_THROW(
                    Ipv4EndpointOption::Deserialize(
                        cPayload, _offset, OptionType::LoadBalancing, false),
                    std::out_of_range);
            }

            TEST(OptionTest, EndpointDeserializationTruncatedPayload)
            {
                // The port number field is missing.
                const std::vector<uint8_t> cPayload{
                    0xc0, 0xa8, 0x00, 0x01,
                    0x00, 0x06};
                std::size_t _offset{0};

                EXPECT_THROW(
                    Ipv4EndpointOption::Deserialize(
                        cPayload, _offset, OptionType::IPv4Endpoint, false),
                    std::out_of_range);
            }

            TEST(OptionTest, LoadBalancingDeserializationTruncatedPayload)
            {
                // The weight field is missing.
                const std::vector<uint8_t> cPayload{0x00, 0x01};
                std::size_t _offset{0};

                EXPECT_THROW(
                    LoadBalancingOption::Deserialize(cPayload, _offset, false),
                    std::out_of_range);
            }
        }
    }
}
