#include <functional>
#include <gtest/gtest.h>
#include <doiplib/diag_message.h>
#include <doiplib/diag_message_ack.h>
#include <doiplib/vehicle_id_request.h>
#include <doiplib/vehicle_id_response.h>
#include "../../../src/application/doip/doip_client.h"
#include "../../../src/application/doip/doip_server.h"

namespace application
{
    namespace doip
    {
        namespace
        {
            const uint8_t cProtocolVersion{0x02};
            const std::string cVin{"WVWZZZ1JZXW000001"};
            const uint16_t cLogicalAddress{0x0e80};
            const uint64_t cEid{0x0000a1b2c3d4e5f6};
            const uint64_t cGid{0x0000f6e5d4c3b2a1};
            const std::string cLocalhost{"127.0.0.1"};
            const std::string cResourcesUrl{"http://127.0.0.1:1/resources"};

            DoipLib::ControllerConfig getControllerConfig()
            {
                DoipLib::ControllerConfig _result;
                _result.protocolVersion = cProtocolVersion;
                _result.doipMaxRequestBytes = DoipServer::cDoipPacketSize;
                _result.doIPInitialVehicleAnnouncementTime = std::chrono::seconds(0);
                _result.doIPVehicleAnnouncementCount = 3;
                _result.doIPVehicleAnnouncementInterval = std::chrono::seconds(1);

                return _result;
            }
        }

        TEST(VehicleIdRequestHandlerTest, GetMessage)
        {
            VehicleIdRequestHandler _handler(
                cProtocolVersion, std::string(cVin), cLogicalAddress, cEid, cGid);

            EXPECT_NE(nullptr, dynamic_cast<DoipLib::VehicleIdRequest *>(_handler.GetMessage()));
        }

        TEST(VehicleIdRequestHandlerTest, HandleVehicleIdRequest)
        {
            VehicleIdRequestHandler _handler(
                cProtocolVersion, std::string(cVin), cLogicalAddress, cEid, cGid);

            const DoipLib::VehicleIdRequest cRequest(cProtocolVersion);
            std::vector<uint8_t> _serializedResponse;
            ASSERT_TRUE(_handler.TryHandle(&cRequest, _serializedResponse));

            DoipLib::VehicleIdResponse _response;
            DoipLib::GenericNackType _nackCode;
            ASSERT_TRUE(_response.TryDeserialize(_serializedResponse, _nackCode));
            EXPECT_EQ(cVin, _response.GetVin());
            EXPECT_EQ(cLogicalAddress, _response.GetLogicalAddress());
            EXPECT_EQ(0x00, _response.GetFurtherAction());
        }

        TEST(VehicleIdRequestHandlerTest, RejectOtherMessages)
        {
            VehicleIdRequestHandler _handler(
                cProtocolVersion, std::string(cVin), cLogicalAddress, cEid, cGid);

            const DoipLib::DiagMessage cRequest(
                cProtocolVersion, cLogicalAddress, cLogicalAddress, {0x22, 0xf5, 0x0d});
            std::vector<uint8_t> _response;
            EXPECT_FALSE(_handler.TryHandle(&cRequest, _response));
        }

        class DiagMessageHandlerTest : public testing::Test
        {
        protected:
            const uint16_t cSourceAddress{0x0e00};
            const uint16_t cTargetAddress{0x0e80};

            helper::CurlWrapper Curl;
            DiagMessageHandler Handler;

            DiagMessageHandlerTest() : Curl("dummy-api-key", "dummy-bearer-token"),
                                       Handler(&Curl, cResourcesUrl, cProtocolVersion)
            {
            }

            bool TryGetUdsResponse(
                std::vector<uint8_t> &&userData,
                std::vector<uint8_t> &udsResponse)
            {
                const DoipLib::DiagMessage cRequest(
                    cProtocolVersion, cSourceAddress, cTargetAddress, std::move(userData));

                std::vector<uint8_t> _serializedResponse;
                if (!Handler.TryHandle(&cRequest, _serializedResponse))
                {
                    return false;
                }

                DoipLib::DiagMessageAck _ack;
                DoipLib::GenericNackType _nackCode;
                if (!_ack.TryDeserialize(_serializedResponse, _nackCode))
                {
                    return false;
                }

                EXPECT_EQ(cSourceAddress, _ack.GetSourceAddress());
                EXPECT_EQ(cTargetAddress, _ack.GetTargetAddress());

                return _ack.TryGetPreviousMessage(udsResponse);
            }
        };

        TEST_F(DiagMessageHandlerTest, GetMessage)
        {
            EXPECT_NE(nullptr, dynamic_cast<DoipLib::DiagMessage *>(Handler.GetMessage()));
        }

        TEST_F(DiagMessageHandlerTest, UnsupportedDid)
        {
            const std::vector<uint8_t> cExpectedResponse{0x7f, 0x22, 0x31};

            std::vector<uint8_t> _udsResponse;
            ASSERT_TRUE(TryGetUdsResponse({0x22, 0xf5, 0xff}, _udsResponse));
            EXPECT_EQ(cExpectedResponse, _udsResponse);
        }

        TEST_F(DiagMessageHandlerTest, UnsupportedService)
        {
            const std::vector<uint8_t> cExpectedResponse{0x7f, 0x10, 0x11};

            std::vector<uint8_t> _udsResponse;
            ASSERT_TRUE(TryGetUdsResponse({0x10, 0x01}, _udsResponse));
            EXPECT_EQ(cExpectedResponse, _udsResponse);
        }

        TEST_F(DiagMessageHandlerTest, EmptyUserData)
        {
            std::vector<uint8_t> _udsResponse;
            EXPECT_FALSE(TryGetUdsResponse({}, _udsResponse));
        }

        TEST_F(DiagMessageHandlerTest, RejectOtherMessages)
        {
            const DoipLib::VehicleIdRequest cRequest(cProtocolVersion);
            std::vector<uint8_t> _response;
            EXPECT_FALSE(Handler.TryHandle(&cRequest, _response));
        }

        class DoipClientServerTest : public testing::Test
        {
        private:
            static const int cMaxPollIterations{200};
            static const int cPollTimeoutMs{10};

        protected:
            AsyncBsdSocketLib::Poller Poller;
            helper::CurlWrapper Curl;

            DoipClientServerTest() : Curl("dummy-api-key", "dummy-bearer-token")
            {
            }

            bool PollUntil(std::function<bool()> predicate)
            {
                for (int i = 0; i < cMaxPollIterations; ++i)
                {
                    if (predicate())
                    {
                        return true;
                    }

                    Poller.TryPoll(cPollTimeoutMs);
                }

                return false;
            }

            std::unique_ptr<DoipServer> CreateServer(uint16_t port)
            {
                return std::unique_ptr<DoipServer>(
                    new DoipServer(
                        &Poller,
                        &Curl,
                        cResourcesUrl,
                        cLocalhost,
                        port,
                        getControllerConfig(),
                        std::string(cVin),
                        cLogicalAddress,
                        cEid,
                        cGid));
            }
        };

        TEST_F(DoipClientServerTest, ServerInvalidIpAddress)
        {
            const uint16_t cPort{18431};
            EXPECT_THROW(
                DoipServer _server(
                    &Poller, &Curl, cResourcesUrl, "192.0.2.1", cPort,
                    getControllerConfig(), std::string(cVin), cLogicalAddress, cEid, cGid),
                std::runtime_error);
        }

        TEST_F(DoipClientServerTest, ClientWithoutServer)
        {
            // Nothing listens on the port, so the TCP connection is refused
            const uint16_t cPort{18432};
            EXPECT_THROW(
                DoipClient _client(
                    &Poller, cLocalhost, cPort, cProtocolVersion,
                    [](std::vector<uint8_t> &&) {}),
                std::runtime_error);
        }

        TEST_F(DoipClientServerTest, DiagMessageBeforeVehicleIdResponse)
        {
            const uint16_t cPort{18433};
            auto _server = CreateServer(cPort);
            DoipClient _client(
                &Poller, cLocalhost, cPort, cProtocolVersion,
                [](std::vector<uint8_t> &&) {});

            // The logical address is unknown before polling for the vehicle ID response
            EXPECT_FALSE(_client.TrySendDiagMessage({0x22, 0xf5, 0xff}));
        }

        TEST_F(DoipClientServerTest, LoopbackDiagMessage)
        {
            const uint16_t cPort{18434};
            const std::vector<uint8_t> cExpectedResponse{0x7f, 0x22, 0x31};

            auto _server = CreateServer(cPort);

            std::vector<std::vector<uint8_t>> _responses;
            DoipClient _client(
                &Poller, cLocalhost, cPort, cProtocolVersion,
                [&](std::vector<uint8_t> &&response)
                { _responses.push_back(std::move(response)); });

            // A diagnostic message can be sent only after the vehicle ID response is handled
            ASSERT_TRUE(PollUntil([&]()
                                  { return _client.TrySendDiagMessage({0x22, 0xf5, 0xff}); }));

            ASSERT_TRUE(PollUntil([&]()
                                  { return !_responses.empty(); }));
            EXPECT_EQ(1U, _responses.size());
            EXPECT_EQ(cExpectedResponse, _responses.front());
        }

        TEST_F(DoipClientServerTest, LoopbackSequentialDiagMessages)
        {
            const uint16_t cPort{18435};
            const std::vector<std::vector<uint8_t>> cRequests{
                {0x22, 0xf5, 0xff},
                {0x10, 0x01}};
            const std::vector<std::vector<uint8_t>> cExpectedResponses{
                {0x7f, 0x22, 0x31},
                {0x7f, 0x10, 0x11}};

            auto _server = CreateServer(cPort);

            std::vector<std::vector<uint8_t>> _responses;
            DoipClient _client(
                &Poller, cLocalhost, cPort, cProtocolVersion,
                [&](std::vector<uint8_t> &&response)
                { _responses.push_back(std::move(response)); });

            for (std::size_t i = 0; i < cRequests.size(); ++i)
            {
                std::vector<uint8_t> _request{cRequests[i]};
                ASSERT_TRUE(PollUntil([&]()
                                      { return _client.TrySendDiagMessage(std::vector<uint8_t>(_request)); }));
                ASSERT_TRUE(PollUntil([&]()
                                      { return _responses.size() == i + 1; }));
            }

            EXPECT_EQ(cExpectedResponses, _responses);
        }
    }
}
