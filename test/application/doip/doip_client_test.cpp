#include <gtest/gtest.h>
#include <sys/socket.h>
#include <asyncbsdsocket/tcp_listener.h>
#include <mutex>
#include "../../../src/application/doip/doip_client.h"
#include "../../../src/application/doip/doip_server.h"
#include "../helper/mock_http_server.h"
#include "../helper/testable_fixture.h"

namespace application
{
    namespace doip
    {
        class DoipClientTest : public testing::Test
        {
        protected:
            static const uint8_t cProtocolVersion{2};
            const std::string cIpAddress{"127.0.0.1"};
            const uint16_t cPort;

            fixture::MockHttpServer HttpServer;
            helper::CurlWrapper Curl;
            AsyncBsdSocketLib::Poller Poller;
            fixture::PollingThread PollingThread;
            std::unique_ptr<DoipServer> Server;
            std::unique_ptr<DoipClient> Client;

            std::mutex Mutex;
            std::vector<std::vector<uint8_t>> Responses;

            DoipClientTest() : cPort{fixture::GetFreePort()},
                               Curl("api-key", "bearer-token"),
                               PollingThread(&Poller)
            {
                HttpServer.SetResponse(
                    "/resources/averageSpeed",
                    "{\"averageSpeed\":{\"value\":\"65\"}}");
            }

            void SetUp() override
            {
                DoipLib::ControllerConfig _config;
                _config.protocolVersion = cProtocolVersion;
                _config.doipMaxRequestBytes = DoipServer::cDoipPacketSize;
                _config.doIPInitialVehicleAnnouncementTime = std::chrono::seconds(1);
                _config.doIPVehicleAnnouncementCount = 3;
                _config.doIPVehicleAnnouncementInterval = std::chrono::seconds(1);

                Server.reset(
                    new DoipServer(
                        &Poller, &Curl, HttpServer.GetUrl("/resources"),
                        cIpAddress, cPort, std::move(_config),
                        "YV1ABCDEFGH123456", 1, 1, 1));

                Client.reset(
                    new DoipClient(
                        &Poller, cIpAddress, cPort, cProtocolVersion,
                        [this](std::vector<uint8_t> &&response)
                        {
                            std::lock_guard<std::mutex> _lock(Mutex);
                            Responses.push_back(std::move(response));
                        }));
            }

            void TearDown() override
            {
                PollingThread.Stop();
                Client.reset();
                Server.reset();
            }

            size_t GetResponseCount()
            {
                std::lock_guard<std::mutex> _lock(Mutex);
                return Responses.size();
            }
        };

        const uint8_t DoipClientTest::cProtocolVersion;

        TEST_F(DoipClientTest, DiagMessageBeforeVehicleIdentification)
        {
            // The logical address is only known after the vehicle ID response is polled
            EXPECT_FALSE(Client->TrySendDiagMessage({0x22, 0xf5, 0x0d}));
        }

        TEST_F(DoipClientTest, DiagMessageRoundTrip)
        {
            PollingThread.Start();
            ASSERT_TRUE(
                fixture::WaitUntil(
                    [this]()
                    { return Client->TrySendDiagMessage({0x22, 0xf5, 0x0d}); }));
            ASSERT_TRUE(
                fixture::WaitUntil(
                    [this]()
                    { return GetResponseCount() > 0; }));
            PollingThread.Stop();

            std::lock_guard<std::mutex> _lock(Mutex);
            const std::vector<uint8_t> cExpected{0x62, 0xf5, 0x0d, 65};
            EXPECT_EQ(cExpected, Responses.front());
        }

        TEST_F(DoipClientTest, NegativeUdsResponse)
        {
            PollingThread.Start();
            ASSERT_TRUE(
                fixture::WaitUntil(
                    [this]()
                    { return Client->TrySendDiagMessage({0x22, 0x12, 0x34}); }));
            ASSERT_TRUE(
                fixture::WaitUntil(
                    [this]()
                    { return GetResponseCount() > 0; }));
            PollingThread.Stop();

            std::lock_guard<std::mutex> _lock(Mutex);
            const std::vector<uint8_t> cExpected{0x7f, 0x22, 0x31};
            EXPECT_EQ(cExpected, Responses.front());
        }

        TEST(DoipClientSetupTest, ConnectionRefused)
        {
            AsyncBsdSocketLib::Poller _poller;
            EXPECT_THROW(
                DoipClient _client(
                    &_poller, "127.0.0.1", fixture::GetFreePort(), 2,
                    [](std::vector<uint8_t> &&) {}),
                std::runtime_error);
        }

        TEST(DoipClientSetupTest, VehicleIdRequestOnConnection)
        {
            const uint16_t cPort{fixture::GetFreePort()};
            AsyncBsdSocketLib::Poller _poller;
            AsyncBsdSocketLib::TcpListener _listener("127.0.0.1", cPort);
            ASSERT_TRUE(_listener.TrySetup());

            {
                DoipClient _client(
                    &_poller, "127.0.0.1", cPort, 2,
                    [](std::vector<uint8_t> &&) {});
                ASSERT_TRUE(_listener.TryAccept());

                fixture::PollingThread _pollingThread(&_poller);
                _pollingThread.Start();

                struct timeval _timeout{5, 0};
                setsockopt(
                    _listener.Connection(), SOL_SOCKET, SO_RCVTIMEO,
                    &_timeout, sizeof(_timeout));
                std::array<uint8_t, 64> _buffer{};
                const ssize_t cReceived{_listener.Receive(_buffer)};
                _pollingThread.Stop();

                ASSERT_GT(cReceived, 0);
                const std::vector<uint8_t> cRequest(_buffer.cbegin(), _buffer.cend());
                DoipLib::PayloadType _payloadType;
                ASSERT_TRUE(DoipLib::Message::TryExtractPayloadType(cRequest, _payloadType));
                EXPECT_EQ(DoipLib::PayloadType::VehicleIdRequest, _payloadType);
            }

            _poller.TryRemoveListener(&_listener);
        }
    }
}
