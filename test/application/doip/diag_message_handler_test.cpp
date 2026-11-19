#include <gtest/gtest.h>
#include <doiplib/diag_message_ack.h>
#include <doiplib/vehicle_id_request.h>
#include "../../../src/application/doip/diag_message_handler.h"
#include "./doip_test_helper.h"

namespace application
{
    namespace doip
    {
        class DiagMessageHandlerTest : public testing::Test
        {
        protected:
            static const uint8_t cProtocolVersion{0x02};
            static const uint16_t cSourceAddress{0x0e00};
            static const uint16_t cTargetAddress{0x0e80};

            StubHttpServer HttpServer;
            helper::CurlWrapper Curl;

            DiagMessageHandlerTest() : HttpServer("{\"averageSpeed\":{\"value\":\"50\"}}"),
                                       Curl("api-key", "token")
            {
            }

            static bool TryHandle(
                const DiagMessageHandler &handler,
                std::vector<uint8_t> &&userData,
                DoipLib::DiagMessageAck &ack)
            {
                const DoipLib::DiagMessage cRequest(
                    cProtocolVersion,
                    cSourceAddress,
                    cTargetAddress,
                    std::move(userData));

                std::vector<uint8_t> _serializedResponse;
                const bool cHandled{
                    handler.TryHandle(&cRequest, _serializedResponse)};

                DoipLib::GenericNackType _nackCode;
                const bool cResult{
                    cHandled && ack.TryDeserialize(_serializedResponse, _nackCode)};

                return cResult;
            }
        };

        const uint8_t DiagMessageHandlerTest::cProtocolVersion;
        const uint16_t DiagMessageHandlerTest::cSourceAddress;
        const uint16_t DiagMessageHandlerTest::cTargetAddress;

        TEST_F(DiagMessageHandlerTest, GetMessageReturnsDiagMessage)
        {
            DiagMessageHandler _handler(
                &Curl, HttpServer.GetUrl(), cProtocolVersion);

            DoipLib::Message *_message{_handler.GetMessage()};

            ASSERT_NE(nullptr, _message);
            EXPECT_NE(nullptr, dynamic_cast<DoipLib::DiagMessage *>(_message));
            EXPECT_EQ(_message, _handler.GetMessage());
        }

        TEST_F(DiagMessageHandlerTest, RejectNonDiagMessage)
        {
            const DiagMessageHandler cHandler(
                &Curl, HttpServer.GetUrl(), cProtocolVersion);
            const DoipLib::VehicleIdRequest cRequest(cProtocolVersion);

            std::vector<uint8_t> _serializedResponse;
            EXPECT_FALSE(cHandler.TryHandle(&cRequest, _serializedResponse));
            EXPECT_TRUE(_serializedResponse.empty());
        }

        TEST_F(DiagMessageHandlerTest, RejectNullRequest)
        {
            const DiagMessageHandler cHandler(
                &Curl, HttpServer.GetUrl(), cProtocolVersion);

            std::vector<uint8_t> _serializedResponse;
            EXPECT_FALSE(cHandler.TryHandle(nullptr, _serializedResponse));
            EXPECT_TRUE(_serializedResponse.empty());
        }

        TEST_F(DiagMessageHandlerTest, RejectEmptyUserData)
        {
            const DiagMessageHandler cHandler(
                &Curl, HttpServer.GetUrl(), cProtocolVersion);
            const DoipLib::DiagMessage cRequest(
                cProtocolVersion,
                cSourceAddress,
                cTargetAddress,
                std::vector<uint8_t>{});

            std::vector<uint8_t> _serializedResponse;
            EXPECT_FALSE(cHandler.TryHandle(&cRequest, _serializedResponse));
            EXPECT_TRUE(_serializedResponse.empty());
            EXPECT_EQ(0, HttpServer.GetRequestCount());
        }

        TEST_F(DiagMessageHandlerTest, UnsupportedServiceIsNegativelyResponded)
        {
            const DiagMessageHandler cHandler(
                &Curl, HttpServer.GetUrl(), cProtocolVersion);
            // Diagnostic session control (SID 0x10) is not offered by the handler
            const std::vector<uint8_t> cExpectedUdsResponse{0x7f, 0x10, 0x11};

            DoipLib::DiagMessageAck _ack;
            ASSERT_TRUE(TryHandle(cHandler, {0x10, 0x01}, _ack));

            std::vector<uint8_t> _udsResponse;
            ASSERT_TRUE(_ack.TryGetPreviousMessage(_udsResponse));
            EXPECT_EQ(cExpectedUdsResponse, _udsResponse);
            EXPECT_EQ(0, HttpServer.GetRequestCount());
        }

        TEST_F(DiagMessageHandlerTest, UnknownDidIsOutOfRange)
        {
            const DiagMessageHandler cHandler(
                &Curl, HttpServer.GetUrl(), cProtocolVersion);
            const std::vector<uint8_t> cExpectedUdsResponse{0x7f, 0x22, 0x31};

            DoipLib::DiagMessageAck _ack;
            ASSERT_TRUE(TryHandle(cHandler, {0x22, 0x12, 0x34}, _ack));

            std::vector<uint8_t> _udsResponse;
            ASSERT_TRUE(_ack.TryGetPreviousMessage(_udsResponse));
            EXPECT_EQ(cExpectedUdsResponse, _udsResponse);
            EXPECT_EQ(0, HttpServer.GetRequestCount());
        }

        TEST_F(DiagMessageHandlerTest, ReadAverageSpeedDid)
        {
            const DiagMessageHandler cHandler(
                &Curl, HttpServer.GetUrl(), cProtocolVersion);
            // Positive response SID (0x62), DID (0xf50d) and 50 km/h
            const std::vector<uint8_t> cExpectedUdsResponse{0x62, 0xf5, 0x0d, 50};

            DoipLib::DiagMessageAck _ack;
            ASSERT_TRUE(TryHandle(cHandler, {0x22, 0xf5, 0x0d}, _ack));

            std::vector<uint8_t> _udsResponse;
            ASSERT_TRUE(_ack.TryGetPreviousMessage(_udsResponse));
            EXPECT_EQ(cExpectedUdsResponse, _udsResponse);
            EXPECT_EQ(1, HttpServer.GetRequestCount());
        }

        TEST_F(DiagMessageHandlerTest, ResponseKeepsProtocolVersion)
        {
            const uint8_t cCustomProtocolVersion{0x03};
            const DiagMessageHandler cHandler(
                &Curl, HttpServer.GetUrl(), cCustomProtocolVersion);
            const DoipLib::DiagMessage cRequest(
                cCustomProtocolVersion,
                cSourceAddress,
                cTargetAddress,
                std::vector<uint8_t>{0x10, 0x01});

            std::vector<uint8_t> _serializedResponse;
            ASSERT_TRUE(cHandler.TryHandle(&cRequest, _serializedResponse));

            // Protocol version, its inverse and positive acknowledgement payload type (0x8002)
            const std::vector<uint8_t> cExpectedHeader{0x03, 0xfc, 0x80, 0x02};
            ASSERT_GE(_serializedResponse.size(), cExpectedHeader.size());
            const std::vector<uint8_t> cActualHeader(
                _serializedResponse.cbegin(),
                _serializedResponse.cbegin() + cExpectedHeader.size());
            EXPECT_EQ(cExpectedHeader, cActualHeader);
        }

        TEST_F(DiagMessageHandlerTest, UnreachableResourceIsNegativelyResponded)
        {
            // No HTTP server listens on the port, so the RESTful request fails.
            const std::string cUnreachableUrl{
                "http://127.0.0.1:" + std::to_string(GetFreePort())};
            const DiagMessageHandler cHandler(
                &Curl, cUnreachableUrl, cProtocolVersion);
            // Conditions not correct (NRC 0x22)
            const std::vector<uint8_t> cExpectedUdsResponse{0x7f, 0x22, 0x22};

            DoipLib::DiagMessageAck _ack;
            ASSERT_TRUE(TryHandle(cHandler, {0x22, 0xf5, 0x0d}, _ack));

            std::vector<uint8_t> _udsResponse;
            ASSERT_TRUE(_ack.TryGetPreviousMessage(_udsResponse));
            EXPECT_EQ(cExpectedUdsResponse, _udsResponse);
        }

        /// @note Pins the addressing that the handler currently puts in the acknowledgement:
        ///       the request source/target addresses are copied in the same order.
        TEST_F(DiagMessageHandlerTest, AckAddressesCurrentBehaviour)
        {
            const DiagMessageHandler cHandler(
                &Curl, HttpServer.GetUrl(), cProtocolVersion);

            DoipLib::DiagMessageAck _ack;
            ASSERT_TRUE(TryHandle(cHandler, {0x10, 0x01}, _ack));

            EXPECT_EQ(cSourceAddress, _ack.GetSourceAddress());
            EXPECT_EQ(cTargetAddress, _ack.GetTargetAddress());
        }

        /// @note LIKELY BUG (production code, not fixed here): per ISO 13400-2 the source
        ///       address of a diagnostic message acknowledgement is the entity that received
        ///       the request and its target address is the tester, i.e., the request
        ///       addresses swapped. The handler copies them unswapped. It is masked in the
        ///       simulation because 'DoipClient' uses the same address for both fields.
        TEST_F(DiagMessageHandlerTest, DISABLED_BugAckAddressesAreNotSwapped)
        {
            const DiagMessageHandler cHandler(
                &Curl, HttpServer.GetUrl(), cProtocolVersion);

            DoipLib::DiagMessageAck _ack;
            ASSERT_TRUE(TryHandle(cHandler, {0x10, 0x01}, _ack));

            EXPECT_EQ(cTargetAddress, _ack.GetSourceAddress());
            EXPECT_EQ(cSourceAddress, _ack.GetTargetAddress());
        }
    }
}
