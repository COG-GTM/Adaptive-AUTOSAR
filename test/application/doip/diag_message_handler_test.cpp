#include <gtest/gtest.h>
#include <doiplib/diag_message_ack.h>
#include <doiplib/vehicle_id_request.h>
#include "../../../src/application/doip/diag_message_handler.h"
#include "../helper/mock_http_server.h"

namespace application
{
    namespace doip
    {
        class DiagMessageHandlerTest : public testing::Test
        {
        protected:
            static const uint8_t cProtocolVersion{2};
            static const uint16_t cSourceAddress{0x0e00};
            static const uint16_t cTargetAddress{0x0001};

            fixture::MockHttpServer Server;
            helper::CurlWrapper Curl;
            DiagMessageHandler Handler;

            DiagMessageHandlerTest() : Curl("api-key", "bearer-token"),
                                       Handler(&Curl, Server.GetUrl("/resources"), cProtocolVersion)
            {
                Server.SetResponse(
                    "/resources/averageSpeed",
                    "{\"averageSpeed\":{\"value\":\"65\"}}");
            }

            bool TryHandle(
                std::vector<uint8_t> &&userData,
                std::vector<uint8_t> &previousMessage)
            {
                const DoipLib::DiagMessage cRequest(
                    cProtocolVersion, cSourceAddress, cTargetAddress, std::move(userData));
                std::vector<uint8_t> _serializedResponse;
                if (!Handler.TryHandle(&cRequest, _serializedResponse))
                {
                    return false;
                }

                DoipLib::DiagMessageAck _ack;
                DoipLib::GenericNackType _nack;
                EXPECT_TRUE(_ack.TryDeserialize(_serializedResponse, _nack));
                EXPECT_EQ(cSourceAddress, _ack.GetSourceAddress());
                EXPECT_EQ(cTargetAddress, _ack.GetTargetAddress());
                EXPECT_TRUE(_ack.TryGetPreviousMessage(previousMessage));

                return true;
            }
        };

        const uint8_t DiagMessageHandlerTest::cProtocolVersion;
        const uint16_t DiagMessageHandlerTest::cSourceAddress;
        const uint16_t DiagMessageHandlerTest::cTargetAddress;

        TEST_F(DiagMessageHandlerTest, GetMessage)
        {
            DoipLib::Message *_message{Handler.GetMessage()};
            ASSERT_NE(nullptr, _message);
            EXPECT_NE(nullptr, dynamic_cast<DoipLib::DiagMessage *>(_message));
        }

        TEST_F(DiagMessageHandlerTest, ReadDataByIdentifierRequest)
        {
            std::vector<uint8_t> _udsResponse;
            ASSERT_TRUE(TryHandle({0x22, 0xf5, 0x0d}, _udsResponse));

            const std::vector<uint8_t> cExpected{0x62, 0xf5, 0x0d, 65};
            EXPECT_EQ(cExpected, _udsResponse);
        }

        TEST_F(DiagMessageHandlerTest, UnsupportedService)
        {
            std::vector<uint8_t> _udsResponse;
            ASSERT_TRUE(TryHandle({0x10, 0x01}, _udsResponse));

            const std::vector<uint8_t> cExpected{0x7f, 0x10, 0x11};
            EXPECT_EQ(cExpected, _udsResponse);
        }

        TEST_F(DiagMessageHandlerTest, EmptyUserData)
        {
            std::vector<uint8_t> _udsResponse;
            EXPECT_FALSE(TryHandle({}, _udsResponse));
        }

        TEST_F(DiagMessageHandlerTest, MalformedRestfulResponse)
        {
            Server.SetResponse("/resources/fuelAmount", "{\"fuelAmount\":{}}");
            std::vector<uint8_t> _udsResponse;
            EXPECT_FALSE(TryHandle({0x22, 0xf5, 0x2f}, _udsResponse));
        }

        TEST_F(DiagMessageHandlerTest, RejectOtherMessages)
        {
            const DoipLib::VehicleIdRequest cRequest(cProtocolVersion);
            std::vector<uint8_t> _response;
            EXPECT_FALSE(Handler.TryHandle(&cRequest, _response));
        }
    }
}
