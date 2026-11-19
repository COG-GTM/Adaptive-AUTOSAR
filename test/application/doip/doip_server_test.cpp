#include <gtest/gtest.h>
#include <asyncbsdsocket/tcp_client.h>
#include <doiplib/diag_message.h>
#include <doiplib/diag_message_ack.h>
#include "../../../src/application/doip/doip_server.h"
#include "./doip_test_helper.h"

namespace application
{
    namespace doip
    {
        class DoipServerTest : public testing::Test
        {
        protected:
            static const uint8_t cProtocolVersion{0x02};
            static const uint16_t cLogicalAddress{0x0e80};
            const std::string cIpAddress{"127.0.0.1"};
            const std::string cVin{"WAUZZZ8V9KA000001"};
            const uint16_t cPort;

            AsyncBsdSocketLib::Poller Poller;
            StubHttpServer HttpServer;
            helper::CurlWrapper Curl;

            DoipServerTest() : cPort{GetFreePort()},
                               HttpServer("{\"averageSpeed\":{\"value\":\"50\"}}"),
                               Curl("api-key", "token")
            {
            }

            DoipServer *CreateServer(std::string ipAddress)
            {
                std::string _vin{cVin};
                DoipServer *_result{
                    new DoipServer(
                        &Poller,
                        &Curl,
                        HttpServer.GetUrl(),
                        ipAddress,
                        cPort,
                        CreateControllerConfig(cProtocolVersion),
                        std::move(_vin),
                        cLogicalAddress,
                        0x01,
                        0x02)};

                return _result;
            }

            /// @brief Send a request via a plain TCP client and poll till the server responds
            bool TryExchange(
                const AsyncBsdSocketLib::TcpClient &client,
                const std::vector<uint8_t> &request,
                std::vector<uint8_t> &response,
                std::chrono::milliseconds timeout = std::chrono::milliseconds(3000))
            {
                std::array<uint8_t, DoipServer::cDoipPacketSize> _sendBuffer{};
                std::copy(request.cbegin(), request.cend(), _sendBuffer.begin());
                if (client.Send(_sendBuffer) < 0)
                {
                    return false;
                }

                std::array<uint8_t, DoipServer::cDoipPacketSize> _receiveBuffer{};
                ssize_t _received{-1};
                const int cConnection{client.Connection()};
                const std::function<bool()> cCondition{
                    [&_receiveBuffer, &_received, cConnection]()
                    {
                        _received =
                            recv(
                                cConnection,
                                _receiveBuffer.data(),
                                _receiveBuffer.size(),
                                MSG_DONTWAIT);
                        return _received > 0;
                    }};

                const bool cResult{PollUntil(Poller, cCondition, timeout)};
                if (cResult)
                {
                    response =
                        std::vector<uint8_t>(
                            _receiveBuffer.cbegin(),
                            _receiveBuffer.cbegin() + _received);
                }

                return cResult;
            }
        };

        const uint8_t DoipServerTest::cProtocolVersion;
        const uint16_t DoipServerTest::cLogicalAddress;

        TEST_F(DoipServerTest, PacketSize)
        {
            const size_t cExpectedResult{64};
            const size_t cActualResult{DoipServer::cDoipPacketSize};
            EXPECT_EQ(cExpectedResult, cActualResult);
        }

        TEST_F(DoipServerTest, ConstructionFailsOnUnavailableAddress)
        {
            // TEST-NET-1 address (RFC 5737) is not assigned to any local interface.
            const std::string cUnavailableIpAddress{"192.0.2.1"};
            EXPECT_THROW(CreateServer(cUnavailableIpAddress), std::runtime_error);
        }

        TEST_F(DoipServerTest, VehicleIdRequestOverTcp)
        {
            std::unique_ptr<DoipServer> _server{CreateServer(cIpAddress)};
            AsyncBsdSocketLib::TcpClient _client(cIpAddress, cPort);
            ASSERT_TRUE(_client.TrySetup());
            ASSERT_TRUE(_client.TryConnect());

            const DoipLib::VehicleIdRequest cRequest(cProtocolVersion);
            std::vector<uint8_t> _serializedRequest;
            cRequest.Serialize(_serializedRequest);

            std::vector<uint8_t> _serializedResponse;
            ASSERT_TRUE(
                TryExchange(_client, _serializedRequest, _serializedResponse));

            DoipLib::VehicleIdResponse _response;
            DoipLib::GenericNackType _nackCode;
            ASSERT_TRUE(
                _response.TryDeserialize(_serializedResponse, _nackCode));
            EXPECT_EQ(cVin, _response.GetVin());
            EXPECT_EQ(cLogicalAddress, _response.GetLogicalAddress());
        }

        TEST_F(DoipServerTest, DiagMessageOverTcp)
        {
            std::unique_ptr<DoipServer> _server{CreateServer(cIpAddress)};
            AsyncBsdSocketLib::TcpClient _client(cIpAddress, cPort);
            ASSERT_TRUE(_client.TrySetup());
            ASSERT_TRUE(_client.TryConnect());

            const DoipLib::DiagMessage cRequest(
                cProtocolVersion,
                cLogicalAddress,
                cLogicalAddress,
                std::vector<uint8_t>{0x22, 0xf5, 0x0d});
            std::vector<uint8_t> _serializedRequest;
            cRequest.Serialize(_serializedRequest);

            std::vector<uint8_t> _serializedResponse;
            ASSERT_TRUE(
                TryExchange(_client, _serializedRequest, _serializedResponse));

            DoipLib::DiagMessageAck _ack;
            DoipLib::GenericNackType _nackCode;
            ASSERT_TRUE(_ack.TryDeserialize(_serializedResponse, _nackCode));

            const std::vector<uint8_t> cExpectedUdsResponse{0x62, 0xf5, 0x0d, 50};
            std::vector<uint8_t> _udsResponse;
            ASSERT_TRUE(_ack.TryGetPreviousMessage(_udsResponse));
            EXPECT_EQ(cExpectedUdsResponse, _udsResponse);
        }

        TEST_F(DoipServerTest, SequentialRequestsOnSameConnection)
        {
            std::unique_ptr<DoipServer> _server{CreateServer(cIpAddress)};
            AsyncBsdSocketLib::TcpClient _client(cIpAddress, cPort);
            ASSERT_TRUE(_client.TrySetup());
            ASSERT_TRUE(_client.TryConnect());

            const DoipLib::VehicleIdRequest cRequest(cProtocolVersion);
            std::vector<uint8_t> _serializedRequest;
            cRequest.Serialize(_serializedRequest);

            std::vector<uint8_t> _firstResponse;
            ASSERT_TRUE(
                TryExchange(_client, _serializedRequest, _firstResponse));

            std::vector<uint8_t> _secondResponse;
            ASSERT_TRUE(
                TryExchange(_client, _serializedRequest, _secondResponse));

            DoipLib::PayloadType _payloadType;
            ASSERT_TRUE(
                DoipLib::Message::TryExtractPayloadType(
                    _secondResponse, _payloadType));
            EXPECT_EQ(
                DoipLib::PayloadType::VehicleAnnoucementIdResponse, _payloadType);
        }

        TEST_F(DoipServerTest, UnsupportedPayloadTypeIsNacked)
        {
            std::unique_ptr<DoipServer> _server{CreateServer(cIpAddress)};
            AsyncBsdSocketLib::TcpClient _client(cIpAddress, cPort);
            ASSERT_TRUE(_client.TrySetup());
            ASSERT_TRUE(_client.TryConnect());

            // Alive check request (payload type 0x0007) has no registered handler.
            const std::vector<uint8_t> cRequest{
                0x02, 0xfd, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00};
            // Generic NACK (payload type 0x0000) with 'unsupported payload type' code (0x01)
            const std::vector<uint8_t> cExpectedResponse{
                0x02, 0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01};

            std::vector<uint8_t> _serializedResponse;
            ASSERT_TRUE(TryExchange(_client, cRequest, _serializedResponse));

            ASSERT_GE(_serializedResponse.size(), cExpectedResponse.size());
            const std::vector<uint8_t> cActualResponse(
                _serializedResponse.cbegin(),
                _serializedResponse.cbegin() + cExpectedResponse.size());
            EXPECT_EQ(cExpectedResponse, cActualResponse);
        }

        TEST_F(DoipServerTest, InvalidProtocolVersionIsNacked)
        {
            std::unique_ptr<DoipServer> _server{CreateServer(cIpAddress)};
            AsyncBsdSocketLib::TcpClient _client(cIpAddress, cPort);
            ASSERT_TRUE(_client.TrySetup());
            ASSERT_TRUE(_client.TryConnect());

            // Vehicle ID request whose inverse protocol version does not match
            const std::vector<uint8_t> cRequest{
                0x02, 0x02, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00};
            // Generic NACK with 'invalid protocol version' code (0x00)
            const std::vector<uint8_t> cExpectedResponse{
                0x02, 0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00};

            std::vector<uint8_t> _serializedResponse;
            ASSERT_TRUE(TryExchange(_client, cRequest, _serializedResponse));

            ASSERT_GE(_serializedResponse.size(), cExpectedResponse.size());
            const std::vector<uint8_t> cActualResponse(
                _serializedResponse.cbegin(),
                _serializedResponse.cbegin() + cExpectedResponse.size());
            EXPECT_EQ(cExpectedResponse, cActualResponse);
        }

        TEST_F(DoipServerTest, UnhandledDiagMessageIsNotResponded)
        {
            std::unique_ptr<DoipServer> _server{CreateServer(cIpAddress)};
            AsyncBsdSocketLib::TcpClient _client(cIpAddress, cPort);
            ASSERT_TRUE(_client.TrySetup());
            ASSERT_TRUE(_client.TryConnect());

            // A diagnostic message without any UDS payload cannot be routed.
            const DoipLib::DiagMessage cRequest(
                cProtocolVersion,
                cLogicalAddress,
                cLogicalAddress,
                std::vector<uint8_t>{});
            std::vector<uint8_t> _serializedRequest;
            cRequest.Serialize(_serializedRequest);

            const std::chrono::milliseconds cTimeout{300};
            std::vector<uint8_t> _serializedResponse;
            EXPECT_FALSE(
                TryExchange(
                    _client, _serializedRequest, _serializedResponse, cTimeout));
        }

        TEST_F(DoipServerTest, DestructionReleasesPollerAndPort)
        {
            std::unique_ptr<DoipServer> _server{CreateServer(cIpAddress)};
            _server.reset();
            _server.reset(CreateServer(cIpAddress));

            AsyncBsdSocketLib::TcpClient _client(cIpAddress, cPort);
            ASSERT_TRUE(_client.TrySetup());
            ASSERT_TRUE(_client.TryConnect());

            const DoipLib::VehicleIdRequest cRequest(cProtocolVersion);
            std::vector<uint8_t> _serializedRequest;
            cRequest.Serialize(_serializedRequest);

            std::vector<uint8_t> _serializedResponse;
            EXPECT_TRUE(
                TryExchange(_client, _serializedRequest, _serializedResponse));
        }
    }
}
