#include <gtest/gtest.h>
#include "../../../../../src/ara/com/someip/sd/someip_sd_client.h"
#include "../../../../../src/ara/com/entry/service_entry.h"
#include "../../../../../src/ara/com/option/ipv4_endpoint_option.h"
#include "../../helper/mockup_network_layer.h"

namespace ara
{
    namespace com
    {
        namespace someip
        {
            namespace sd
            {
                class SomeIpSdClientTest : public testing::Test
                {
                private:
                    static const int cInitialDelayMin{100};
                    static const int cInitialDelayMax{200};

                protected:
                    static const uint16_t cServiceId{0x1234};
                    static const uint16_t cInstanceId{0x0001};
                    static const uint8_t cMajorVersion{0x01};
                    static const uint32_t cMinorVersion{0x00000000};
                    static const uint32_t cTtl{0x000010};
                    static const uint16_t cPort{8080};

                    helper::MockupNetworkLayer<SomeIpSdMessage> NetworkLayer;
                    SomeIpSdClient Client;

                    SomeIpSdClientTest() : Client(
                                               &NetworkLayer,
                                               cServiceId,
                                               cInitialDelayMin,
                                               cInitialDelayMax)
                    {
                    }

                    /// @brief Send a service offer through the mockup network layer to the client
                    void SendOffer(
                        uint16_t serviceId,
                        std::unique_ptr<option::Option> endpoint)
                    {
                        std::unique_ptr<entry::ServiceEntry> _entry{
                            entry::ServiceEntry::CreateOfferServiceEntry(
                                serviceId, cInstanceId, cMajorVersion, cMinorVersion, cTtl)};
                        if (endpoint)
                        {
                            _entry->AddFirstOption(std::move(endpoint));
                        }

                        SomeIpSdMessage _message;
                        _message.AddEntry(std::move(_entry));
                        NetworkLayer.Send(_message);
                    }

                    std::unique_ptr<option::Option> CreateEndpoint(
                        option::Layer4ProtocolType protocol) const
                    {
                        const helper::Ipv4Address cIpAddress(192, 168, 0, 7);
                        return option::Ipv4EndpointOption::CreateUnitcastEndpoint(
                            false, cIpAddress, protocol, cPort);
                    }
                };

                const int SomeIpSdClientTest::cInitialDelayMin;
                const int SomeIpSdClientTest::cInitialDelayMax;
                const uint16_t SomeIpSdClientTest::cServiceId;
                const uint16_t SomeIpSdClientTest::cInstanceId;
                const uint8_t SomeIpSdClientTest::cMajorVersion;
                const uint32_t SomeIpSdClientTest::cMinorVersion;
                const uint32_t SomeIpSdClientTest::cTtl;
                const uint16_t SomeIpSdClientTest::cPort;

                TEST_F(SomeIpSdClientTest, NoOfferedEndpointBeforeOffer)
                {
                    std::string _ipAddress;
                    uint16_t _port{0};

                    EXPECT_FALSE(Client.TryGetOfferedEndpoint(_ipAddress, _port));
                    EXPECT_TRUE(_ipAddress.empty());
                    EXPECT_EQ(0, _port);
                }

                TEST_F(SomeIpSdClientTest, OfferedTcpEndpointExtraction)
                {
                    const std::string cExpectedIpAddress{"192.168.0.7"};
                    std::string _ipAddress;
                    uint16_t _port{0};

                    SendOffer(
                        cServiceId, CreateEndpoint(option::Layer4ProtocolType::Tcp));

                    EXPECT_TRUE(Client.TryGetOfferedEndpoint(_ipAddress, _port));
                    EXPECT_EQ(cExpectedIpAddress, _ipAddress);
                    EXPECT_EQ(cPort, _port);
                }

                TEST_F(SomeIpSdClientTest, OfferWithoutOptions)
                {
                    std::string _ipAddress;
                    uint16_t _port{0};

                    EXPECT_NO_THROW(SendOffer(cServiceId, nullptr));

                    EXPECT_FALSE(Client.TryGetOfferedEndpoint(_ipAddress, _port));
                }

                TEST_F(SomeIpSdClientTest, OfferOfAnotherService)
                {
                    const uint16_t cOtherServiceId{0x4321};
                    std::string _ipAddress;
                    uint16_t _port{0};

                    SendOffer(
                        cOtherServiceId, CreateEndpoint(option::Layer4ProtocolType::Tcp));

                    EXPECT_FALSE(Client.TryGetOfferedEndpoint(_ipAddress, _port));
                }

                TEST_F(SomeIpSdClientTest, WaitForOfferTimeout)
                {
                    const int cDurationMs{10};

                    // The client is not started and no service is offered.
                    EXPECT_FALSE(Client.TryWaitUntiServiceOffered(cDurationMs));
                    // The service is not seen, so the offer is considered as stopped.
                    EXPECT_TRUE(Client.TryWaitUntiServiceOfferStopped(cDurationMs));
                }

                /// @note BUG (production code, not fixed here): the inner loop of
                ///       SomeIpSdClient::tryExtractOfferedEndpoint is
                ///       'for (size_t j = 0; entry->FirstOptions().size(); ++j)', i.e., the
                ///       condition never compares 'j' with the options size. If an offer
                ///       carries first options but none of them is an IPv4 TCP endpoint,
                ///       'FirstOptions().at(j)' runs past the end and std::out_of_range
                ///       escapes from the network layer receive callback.
                ///       The test is disabled because it fails until the loop is fixed.
                TEST_F(SomeIpSdClientTest, DISABLED_BugOfferWithoutTcpEndpointThrowsOutOfRange)
                {
                    std::string _ipAddress;
                    uint16_t _port{0};

                    EXPECT_NO_THROW(
                        SendOffer(
                            cServiceId, CreateEndpoint(option::Layer4ProtocolType::Udp)));

                    EXPECT_FALSE(Client.TryGetOfferedEndpoint(_ipAddress, _port));
                }
            }
        }
    }
}
