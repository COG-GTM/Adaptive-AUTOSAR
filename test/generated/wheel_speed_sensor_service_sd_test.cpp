#include <gtest/gtest.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <array>
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include "../../src/ara/com/option/ipv4_endpoint_option.h"
#include "../../src/ara/com/someip/sd/sd_network_layer.h"
#include "../../src/ara/com/someip/sd/someip_sd_client.h"
#include "../../src/ara/com/someip/sd/someip_sd_server.h"
#include "../../src/generated/wheel_speed_sensor_service/wheel_speed_sensor_service_common.h"

namespace generated
{
    namespace wheelspeedsensorservice
    {
        /// @brief Hand-written SOME/IP-SD wiring of the generated service identifiers
        /// @note The generated skeleton does not drive service discovery itself.
        class WheelSpeedSensorServiceSdTest : public testing::Test
        {
        private:
            int mObserverSocket;

        protected:
            static const std::string cNicIpAddress;
            static const std::string cMulticastGroup;
            static const std::uint16_t cSdPort{30490};
            static const std::uint16_t cEventPort{30509};
            static const std::uint16_t cInstanceId{1};
            static const int cWaitDuration{5000};
            static const int cClientStopWaitDuration{500};

            WheelSpeedSensorServiceSdTest() : mObserverSocket{-1}
            {
            }

            /// @brief Open a plain UDP socket (independent of ara::com) on the SD multicast group
            bool TryOpenObserver()
            {
                mObserverSocket = socket(AF_INET, SOCK_DGRAM, 0);
                if (mObserverSocket < 0)
                {
                    return false;
                }

                const int cEnable{1};
                struct timeval _timeout;
                _timeout.tv_sec = 0;
                _timeout.tv_usec = 100000;
                struct sockaddr_in _address {};
                _address.sin_family = AF_INET;
                _address.sin_addr.s_addr = htonl(INADDR_ANY);
                _address.sin_port = htons(cSdPort);
                struct ip_mreq _membership;
                _membership.imr_interface.s_addr = inet_addr(cNicIpAddress.c_str());
                _membership.imr_multiaddr.s_addr = inet_addr(cMulticastGroup.c_str());

                return setsockopt(
                           mObserverSocket, SOL_SOCKET, SO_REUSEADDR,
                           &cEnable, sizeof(cEnable)) == 0 &&
                       setsockopt(
                           mObserverSocket, SOL_SOCKET, SO_RCVTIMEO,
                           &_timeout, sizeof(_timeout)) == 0 &&
                       bind(
                           mObserverSocket,
                           reinterpret_cast<struct sockaddr *>(&_address),
                           sizeof(_address)) == 0 &&
                       setsockopt(
                           mObserverSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                           &_membership, sizeof(_membership)) == 0;
            }

            /// @brief Wait for an offer (TTL > 0) or a stop offer (TTL = 0) datagram of the generated service
            bool TryObserveOffer(
                bool stopOffer, std::string &sourceIpAddress, std::uint16_t &offeredPort)
            {
                const std::chrono::steady_clock::time_point cDeadline{
                    std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(cWaitDuration)};

                while (std::chrono::steady_clock::now() < cDeadline)
                {
                    std::array<std::uint8_t, 512> _buffer;
                    struct sockaddr_in _source {};
                    socklen_t _sourceLength{sizeof(_source)};
                    const ssize_t cReceivedSize{
                        recvfrom(
                            mObserverSocket, _buffer.data(), _buffer.size(), 0,
                            reinterpret_cast<struct sockaddr *>(&_source),
                            &_sourceLength)};
                    if (cReceivedSize <= 0)
                    {
                        continue;
                    }

                    const std::vector<std::uint8_t> cPayload(
                        _buffer.cbegin(), _buffer.cbegin() + cReceivedSize);
                    const ara::com::someip::sd::SomeIpSdMessage cMessage{
                        ara::com::someip::sd::SomeIpSdMessage::Deserialize(cPayload)};

                    for (const std::unique_ptr<ara::com::entry::Entry> &entry : cMessage.Entries())
                    {
                        if (entry->Type() != ara::com::entry::EntryType::Offering ||
                            entry->ServiceId() != cServiceId ||
                            entry->InstanceId() != cInstanceId ||
                            entry->MajorVersion() != cMajorVersion ||
                            (entry->TTL() == 0) != stopOffer)
                        {
                            continue;
                        }

                        for (const std::unique_ptr<ara::com::option::Option> &option : entry->FirstOptions())
                        {
                            const ara::com::option::Ipv4EndpointOption *cEndpoint{
                                dynamic_cast<const ara::com::option::Ipv4EndpointOption *>(
                                    option.get())};
                            if (cEndpoint)
                            {
                                offeredPort = cEndpoint->Port();
                            }
                        }

                        sourceIpAddress = inet_ntoa(_source.sin_addr);
                        return true;
                    }
                }

                return false;
            }

            void TearDown() override
            {
                if (mObserverSocket >= 0)
                {
                    close(mObserverSocket);
                }
            }
        };

        const std::string WheelSpeedSensorServiceSdTest::cNicIpAddress{"127.0.0.1"};
        const std::string WheelSpeedSensorServiceSdTest::cMulticastGroup{"239.0.0.1"};
        const std::uint16_t WheelSpeedSensorServiceSdTest::cSdPort;
        const std::uint16_t WheelSpeedSensorServiceSdTest::cEventPort;
        const std::uint16_t WheelSpeedSensorServiceSdTest::cInstanceId;
        const int WheelSpeedSensorServiceSdTest::cWaitDuration;

        TEST_F(WheelSpeedSensorServiceSdTest, OfferOnLoopback)
        {
            const int cInitialDelayMin{10};
            const int cInitialDelayMax{20};
            const int cRepetitionBaseDelay{30};
            const int cCycleOfferDelay{100};
            const std::uint32_t cRepetitionMax{1};

            AsyncBsdSocketLib::Poller _poller;
            std::unique_ptr<ara::com::someip::sd::SdNetworkLayer> _serverNetworkLayer;
            std::unique_ptr<ara::com::someip::sd::SdNetworkLayer> _clientNetworkLayer;

            try
            {
                _serverNetworkLayer.reset(
                    new ara::com::someip::sd::SdNetworkLayer(
                        &_poller, cNicIpAddress, cMulticastGroup, cSdPort));
                _clientNetworkLayer.reset(
                    new ara::com::someip::sd::SdNetworkLayer(
                        &_poller, cNicIpAddress, cMulticastGroup, cSdPort));
            }
            catch (const std::runtime_error &)
            {
                GTEST_SKIP() << "UDP multicast is not available on the loopback interface.";
            }

            if (!TryOpenObserver())
            {
                GTEST_SKIP() << "UDP multicast is not available on the loopback interface.";
            }

            std::atomic_bool _polling{true};
            std::thread _pollingThread(
                [&_poller, &_polling]()
                {
                    const int cPollingTimeout{10};
                    while (_polling)
                    {
                        _poller.TryPoll(cPollingTimeout);
                    }
                });

            {
                const ara::com::helper::Ipv4Address cEndpointIp(cNicIpAddress);
                ara::com::someip::sd::SomeIpSdServer _server(
                    _serverNetworkLayer.get(),
                    cServiceId,
                    cInstanceId,
                    cMajorVersion,
                    cMinorVersion,
                    cEndpointIp,
                    cEventPort,
                    cInitialDelayMin,
                    cInitialDelayMax,
                    cRepetitionBaseDelay,
                    cCycleOfferDelay,
                    cRepetitionMax);
                ara::com::someip::sd::SomeIpSdClient _client(
                    _clientNetworkLayer.get(),
                    cServiceId,
                    cInitialDelayMin,
                    cInitialDelayMax,
                    cRepetitionBaseDelay,
                    cRepetitionMax);

                _client.Start();
                _server.Start();

                // The SD client discovers the offered service.
                // SomeIpSdClient::TryGetOfferedEndpoint is not used: it is not safe
                // to call while offers are being received (shared lock object).
                EXPECT_TRUE(_client.TryWaitUntiServiceOffered(cWaitDuration));

                // The offer is also visible on the wire for a socket outside ara::com
                std::string _sourceIpAddress;
                std::uint16_t _observedPort{0};
                EXPECT_TRUE(TryObserveOffer(false, _sourceIpAddress, _observedPort));
                EXPECT_EQ(cNicIpAddress, _sourceIpAddress);
                EXPECT_EQ(cEventPort, _observedPort);

                _server.Stop();
                EXPECT_TRUE(TryObserveOffer(true, _sourceIpAddress, _observedPort));
                // The client wait only returns early if it starts before the stop offer arrives
                EXPECT_TRUE(_client.TryWaitUntiServiceOfferStopped(cClientStopWaitDuration));

                _client.Stop();
            }

            _polling = false;
            _pollingThread.join();
        }
    }
}
