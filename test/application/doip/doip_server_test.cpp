#include <gtest/gtest.h>
#include <sys/socket.h>
#include <asyncbsdsocket/tcp_client.h>
#include <doiplib/diag_message_ack.h>
#include <doiplib/generic_nack.h>
#include <doiplib/vehicle_id_request.h>
#include <doiplib/vehicle_id_response.h>
#include "../../../src/application/doip/doip_server.h"
#include "../helper/mock_http_server.h"
#include "../helper/testable_fixture.h"

namespace application
{
    namespace doip
    {
        namespace
        {
            DoipLib::ControllerConfig GetConfig()
            {
                DoipLib::ControllerConfig _result;
                _result.protocolVersion = 2;
                _result.doipMaxRequestBytes = DoipServer::cDoipPacketSize;
                _result.doIPInitialVehicleAnnouncementTime = std::chrono::seconds(1);
                _result.doIPVehicleAnnouncementCount = 3;
                _result.doIPVehicleAnnouncementInterval = std::chrono::seconds(1);
                return _result;
            }
        }

        class DoipServerTest : public testing::Test
        {
        protected:
            static const uint8_t cProtocolVersion{2};
            static const uint16_t cLogicalAddress{1};
            const std::string cIpAddress{"127.0.0.1"};
            const std::string cVin{"YV1ABCDEFGH123456"};
            const uint16_t cPort;

            fixture::MockHttpServer HttpServer;
            helper::CurlWrapper Curl;
            AsyncBsdSocketLib::Poller Poller;
            fixture::PollingThread PollingThread;
            std::unique_ptr<DoipServer> Server;
            std::unique_ptr<AsyncBsdSocketLib::TcpClient> Client;

            DoipServerTest() : cPort{fixture::GetFreePort()},
                               Curl("api-key", "bearer-token"),
                               PollingThread(&Poller)
            {
                HttpServer.SetResponse(
                    "/resources/averageSpeed",
                    "{\"averageSpeed\":{\"value\":\"65\"}}");
            }

            void SetUp() override
            {
                Server.reset(
                    new DoipServer(
                        &Poller, &Curl, HttpServer.GetUrl("/resources"),
                        cIpAddress, cPort, GetConfig(), std::string(cVin),
                        cLogicalAddress, 1, 1));

                Client.reset(new AsyncBsdSocketLib::TcpClient(cIpAddress, cPort));
                ASSERT_TRUE(Client->TrySetup());
                ASSERT_TRUE(Client->TryConnect());
                struct timeval _timeout{5, 0};
                setsockopt(
                    Client->Descriptor(), SOL_SOCKET, SO_RCVTIMEO,
                    &_timeout, sizeof(_timeout));

                PollingThread.Start();
            }

            void TearDown() override
            {
                PollingThread.Stop();
                Client.reset();
                Server.reset();
            }

            std::vector<uint8_t> Exchange(const DoipLib::Message &request)
            {
                std::vector<uint8_t> _serializedRequest;
                request.Serialize(_serializedRequest);
                std::array<uint8_t, DoipServer::cDoipPacketSize> _sendBuffer{};
                std::copy(
                    _serializedRequest.cbegin(), _serializedRequest.cend(),
                    _sendBuffer.begin());
                EXPECT_EQ(_sendBuffer.size(), Client->Send(_sendBuffer));

                std::array<uint8_t, DoipServer::cDoipPacketSize> _receiveBuffer{};
                const ssize_t cReceived{Client->Receive(_receiveBuffer)};
                EXPECT_GT(cReceived, 0);

                return std::vector<uint8_t>(
                    _receiveBuffer.cbegin(), _receiveBuffer.cend());
            }
        };

        const uint8_t DoipServerTest::cProtocolVersion;
        const uint16_t DoipServerTest::cLogicalAddress;

        TEST_F(DoipServerTest, VehicleIdRequest)
        {
            const DoipLib::VehicleIdRequest cRequest(cProtocolVersion);
            const std::vector<uint8_t> cSerializedResponse{Exchange(cRequest)};

            DoipLib::VehicleIdResponse _response;
            DoipLib::GenericNackType _nack;
            ASSERT_TRUE(_response.TryDeserialize(cSerializedResponse, _nack));
            EXPECT_EQ(cVin, _response.GetVin());
            EXPECT_EQ(cLogicalAddress, _response.GetLogicalAddress());
        }

        TEST_F(DoipServerTest, DiagMessage)
        {
            const DoipLib::DiagMessage cRequest(
                cProtocolVersion, cLogicalAddress, cLogicalAddress,
                std::vector<uint8_t>{0x22, 0xf5, 0x0d});
            const std::vector<uint8_t> cSerializedResponse{Exchange(cRequest)};

            DoipLib::DiagMessageAck _ack;
            DoipLib::GenericNackType _nack;
            ASSERT_TRUE(_ack.TryDeserialize(cSerializedResponse, _nack));

            std::vector<uint8_t> _udsResponse;
            ASSERT_TRUE(_ack.TryGetPreviousMessage(_udsResponse));
            const std::vector<uint8_t> cExpected{0x62, 0xf5, 0x0d, 65};
            EXPECT_EQ(cExpected, _udsResponse);
        }

        TEST_F(DoipServerTest, UnsupportedPayloadType)
        {
            const DoipLib::GenericNack cRequest(
                cProtocolVersion, DoipLib::GenericNackType::InvalidPayloadLength);
            const std::vector<uint8_t> cSerializedResponse{Exchange(cRequest)};

            DoipLib::PayloadType _payloadType;
            ASSERT_TRUE(
                DoipLib::Message::TryExtractPayloadType(cSerializedResponse, _payloadType));
            EXPECT_EQ(DoipLib::PayloadType::GenericNegativeAcknowledgement, _payloadType);
        }

        TEST(DoipServerSetupTest, InvalidIpAddress)
        {
            helper::CurlWrapper _curl("api-key", "bearer-token");
            AsyncBsdSocketLib::Poller _poller;
            DoipLib::ControllerConfig _config{GetConfig()};

            EXPECT_THROW(
                DoipServer _server(
                    &_poller, &_curl, "http://127.0.0.1/resources",
                    "192.0.2.1", fixture::GetFreePort(), std::move(_config),
                    "YV1ABCDEFGH123456", 1, 1, 1),
                std::runtime_error);
        }
    }
}
