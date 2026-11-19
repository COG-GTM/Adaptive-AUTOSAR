#include <gtest/gtest.h>
#include <asyncbsdsocket/poller.h>
#include "../../../../../src/ara/com/someip/sd/sd_network_layer.h"
#include "../../../../../src/ara/com/entry/service_entry.h"

namespace ara
{
    namespace com
    {
        namespace someip
        {
            namespace sd
            {
                /// @note The tests use a real UDP multicast socket over the loopback interface.
                class SdNetworkLayerTest : public testing::Test
                {
                protected:
                    static const std::string cNicIpAddress;
                    static const std::string cMulticastGroup;
                    static const int cPollingTimeoutMs{10};
                    static const int cPollingRetries{200};

                    AsyncBsdSocketLib::Poller Poller;
                };

                const std::string SdNetworkLayerTest::cNicIpAddress{"127.0.0.1"};
                const std::string SdNetworkLayerTest::cMulticastGroup{"239.0.0.1"};
                const int SdNetworkLayerTest::cPollingTimeoutMs;
                const int SdNetworkLayerTest::cPollingRetries;

                TEST_F(SdNetworkLayerTest, Constructor)
                {
                    const uint16_t cPort{39401};

                    EXPECT_NO_THROW(
                        SdNetworkLayer _networkLayer(
                            &Poller, cNicIpAddress, cMulticastGroup, cPort));
                }

                TEST_F(SdNetworkLayerTest, SetupFailure)
                {
                    // 192.168.0.1 is not a multicast group, so joining it should fail.
                    const std::string cInvalidMulticastGroup{"192.168.0.1"};
                    const uint16_t cPort{39402};

                    EXPECT_THROW(
                        SdNetworkLayer _networkLayer(
                            &Poller, cNicIpAddress, cInvalidMulticastGroup, cPort),
                        std::runtime_error);
                }

                TEST_F(SdNetworkLayerTest, MulticastLoopbackScenario)
                {
                    const uint16_t cPort{39403};
                    const uint16_t cServiceId{0x1234};
                    const uint16_t cInstanceId{0x0001};
                    const uint8_t cMajorVersion{0x02};
                    const uint32_t cMinorVersion{0x00000003};
                    const uint32_t cTtl{0x000010};

                    SdNetworkLayer _networkLayer(
                        &Poller, cNicIpAddress, cMulticastGroup, cPort);

                    size_t _receivedMessages{0};
                    size_t _receivedEntries{0};
                    uint16_t _receivedServiceId{0};
                    uint32_t _receivedTtl{0};
                    const std::function<void(SomeIpSdMessage)> cReceiver{
                        [&](SomeIpSdMessage message)
                        {
                            ++_receivedMessages;
                            _receivedEntries = message.Entries().size();
                            if (_receivedEntries > 0)
                            {
                                _receivedServiceId =
                                    message.Entries().front()->ServiceId();
                                _receivedTtl = message.Entries().front()->TTL();
                            }
                        }};
                    _networkLayer.SetReceiver(this, cReceiver);

                    SomeIpSdMessage _message;
                    _message.AddEntry(
                        entry::ServiceEntry::CreateOfferServiceEntry(
                            cServiceId, cInstanceId, cMajorVersion, cMinorVersion, cTtl));
                    _networkLayer.Send(_message);

                    for (int i = 0; i < cPollingRetries && _receivedMessages == 0; ++i)
                    {
                        Poller.TryPoll(cPollingTimeoutMs);
                    }

                    ASSERT_GE(_receivedMessages, 1U);
                    EXPECT_EQ(1U, _receivedEntries);
                    EXPECT_EQ(cServiceId, _receivedServiceId);
                    EXPECT_EQ(cTtl, _receivedTtl);
                }
            }
        }
    }
}
