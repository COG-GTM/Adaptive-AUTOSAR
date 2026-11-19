#include <gtest/gtest.h>
#include "../../../src/application/doip/doip_client.h"
#include "../../../src/application/doip/doip_server.h"
#include "./doip_test_helper.h"

namespace application
{
    namespace doip
    {
        class DoipClientTest : public testing::Test
        {
        protected:
            static const uint8_t cProtocolVersion{0x02};
            static const uint16_t cLogicalAddress{0x0e80};
            const std::string cIpAddress{"127.0.0.1"};
            const uint16_t cPort;

            AsyncBsdSocketLib::Poller Poller;
            StubHttpServer HttpServer;
            helper::CurlWrapper Curl;
            std::unique_ptr<DoipServer> Server;
            std::vector<std::vector<uint8_t>> ReceivedUdsResponses;

            DoipClientTest() : cPort{GetFreePort()},
                               HttpServer("{\"averageSpeed\":{\"value\":\"50\"}}"),
                               Curl("api-key", "token")
            {
                std::string _vin{"WAUZZZ8V9KA000001"};
                Server.reset(
                    new DoipServer(
                        &Poller,
                        &Curl,
                        HttpServer.GetUrl(),
                        cIpAddress,
                        cPort,
                        CreateControllerConfig(cProtocolVersion),
                        std::move(_vin),
                        cLogicalAddress,
                        0x01,
                        0x02));
            }

            DoipClient *CreateClient(uint16_t port)
            {
                std::function<void(std::vector<uint8_t> &&)> _callback{
                    [this](std::vector<uint8_t> &&udsResponse)
                    {
                        ReceivedUdsResponses.push_back(std::move(udsResponse));
                    }};

                DoipClient *_result{
                    new DoipClient(
                        &Poller,
                        cIpAddress,
                        port,
                        cProtocolVersion,
                        std::move(_callback))};

                return _result;
            }

            /// @brief Poll till the client fetches the logical address and queues the diagnostic message
            bool TrySend(DoipClient &client, const std::vector<uint8_t> &userData)
            {
                const std::function<bool()> cCondition{
                    [&client, &userData]()
                    {
                        std::vector<uint8_t> _userData{userData};
                        return client.TrySendDiagMessage(std::move(_userData));
                    }};

                return PollUntil(Poller, cCondition);
            }

            bool WaitForResponses(size_t count)
            {
                const std::function<bool()> cCondition{
                    [this, count]()
                    {
                        return ReceivedUdsResponses.size() >= count;
                    }};

                return PollUntil(Poller, cCondition);
            }
        };

        const uint8_t DoipClientTest::cProtocolVersion;
        const uint16_t DoipClientTest::cLogicalAddress;

        TEST_F(DoipClientTest, ConstructionFailsWithoutServer)
        {
            const uint16_t cClosedPort{GetFreePort()};
            EXPECT_THROW(CreateClient(cClosedPort), std::runtime_error);
        }

        TEST_F(DoipClientTest, CannotSendBeforeVehicleIdentification)
        {
            std::unique_ptr<DoipClient> _client{CreateClient(cPort)};

            // No poll has been run yet, so the logical address is still unknown.
            EXPECT_FALSE(_client->TrySendDiagMessage({0x22, 0xf5, 0x0d}));
            EXPECT_TRUE(ReceivedUdsResponses.empty());
        }

        TEST_F(DoipClientTest, SendAfterVehicleIdentification)
        {
            std::unique_ptr<DoipClient> _client{CreateClient(cPort)};

            EXPECT_TRUE(TrySend(*_client, {0x22, 0xf5, 0x0d}));
        }

        TEST_F(DoipClientTest, PositiveUdsResponseIsCalledBack)
        {
            std::unique_ptr<DoipClient> _client{CreateClient(cPort)};
            const std::vector<uint8_t> cExpectedUdsResponse{0x62, 0xf5, 0x0d, 50};

            ASSERT_TRUE(TrySend(*_client, {0x22, 0xf5, 0x0d}));
            ASSERT_TRUE(WaitForResponses(1));

            EXPECT_EQ(cExpectedUdsResponse, ReceivedUdsResponses.at(0));
            EXPECT_EQ(1, HttpServer.GetRequestCount());
        }

        TEST_F(DoipClientTest, NegativeUdsResponseIsCalledBack)
        {
            std::unique_ptr<DoipClient> _client{CreateClient(cPort)};
            // Service not supported (NRC 0x11) for the diagnostic session control
            const std::vector<uint8_t> cExpectedUdsResponse{0x7f, 0x10, 0x11};

            ASSERT_TRUE(TrySend(*_client, {0x10, 0x01}));
            ASSERT_TRUE(WaitForResponses(1));

            EXPECT_EQ(cExpectedUdsResponse, ReceivedUdsResponses.at(0));
        }

        TEST_F(DoipClientTest, SequentialDiagMessages)
        {
            std::unique_ptr<DoipClient> _client{CreateClient(cPort)};
            const std::vector<uint8_t> cFirstExpectedResponse{0x7f, 0x10, 0x11};
            const std::vector<uint8_t> cSecondExpectedResponse{0x7f, 0x22, 0x31};
            const std::vector<uint8_t> cThirdExpectedResponse{0x62, 0xf5, 0x0d, 50};

            ASSERT_TRUE(TrySend(*_client, {0x10, 0x01}));
            ASSERT_TRUE(WaitForResponses(1));
            ASSERT_TRUE(TrySend(*_client, {0x22, 0x12, 0x34}));
            ASSERT_TRUE(WaitForResponses(2));
            ASSERT_TRUE(TrySend(*_client, {0x22, 0xf5, 0x0d}));
            ASSERT_TRUE(WaitForResponses(3));

            EXPECT_EQ(cFirstExpectedResponse, ReceivedUdsResponses.at(0));
            EXPECT_EQ(cSecondExpectedResponse, ReceivedUdsResponses.at(1));
            EXPECT_EQ(cThirdExpectedResponse, ReceivedUdsResponses.at(2));
        }

        TEST_F(DoipClientTest, OversizedDiagMessageIsDropped)
        {
            std::unique_ptr<DoipClient> _client{CreateClient(cPort)};
            // 8 bytes header + 4 bytes addresses + 60 bytes user data exceed the 64 bytes packet size.
            const std::vector<uint8_t> cOversizedUserData(60, 0x22);
            const std::vector<uint8_t> cExpectedUdsResponse{0x7f, 0x10, 0x11};

            // The message is queued, but it is discarded while sending.
            ASSERT_TRUE(TrySend(*_client, cOversizedUserData));
            const std::chrono::milliseconds cSettleTime{200};
            PollFor(Poller, cSettleTime);
            EXPECT_TRUE(ReceivedUdsResponses.empty());

            // The client is still functional after dropping the message.
            ASSERT_TRUE(TrySend(*_client, {0x10, 0x01}));
            ASSERT_TRUE(WaitForResponses(1));
            EXPECT_EQ(cExpectedUdsResponse, ReceivedUdsResponses.at(0));
        }

        TEST_F(DoipClientTest, UnroutableDiagMessageIsNotCalledBack)
        {
            std::unique_ptr<DoipClient> _client{CreateClient(cPort)};

            // The server cannot route a diagnostic message without UDS payload.
            ASSERT_TRUE(TrySend(*_client, {}));
            const std::chrono::milliseconds cSettleTime{200};
            PollFor(Poller, cSettleTime);

            EXPECT_TRUE(ReceivedUdsResponses.empty());
        }

        TEST_F(DoipClientTest, DestructionReleasesPoller)
        {
            std::unique_ptr<DoipClient> _client{CreateClient(cPort)};
            ASSERT_TRUE(TrySend(*_client, {0x10, 0x01}));
            ASSERT_TRUE(WaitForResponses(1));

            _client.reset();

            // Polling after the client destruction should not invoke the dangling delegates.
            const std::chrono::milliseconds cSettleTime{50};
            PollFor(Poller, cSettleTime);
            EXPECT_EQ(1u, ReceivedUdsResponses.size());
        }
    }
}
