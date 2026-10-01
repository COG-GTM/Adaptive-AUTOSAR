#include <gtest/gtest.h>
#include <doiplib/diag_message_ack.h>
#include <doiplib/vehicle_id_request.h>
#include "../../../src/application/doip/diag_message_handler.h"

namespace application
{
    namespace doip
    {
        class DiagMessageHandlerTest : public testing::Test
        {
        private:
            helper::CurlWrapper mCurl;

        protected:
            static const uint8_t cProtocolVersion{0x02};
            static const uint16_t cSourceAddress{0x0e00};
            static const uint16_t cTargetAddress{0x0e80};
            static const uint8_t cNegativeResponseSid{0x7f};
            const std::string cResourcesUrl{"http://127.0.0.1:1/resources"};

            DiagMessageHandler Handler;

            DiagMessageHandlerTest() : mCurl("dummy-api-key", "dummy-bearer-token"),
                                       Handler(&mCurl, cResourcesUrl, cProtocolVersion)
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

        const uint8_t DiagMessageHandlerTest::cProtocolVersion;
        const uint16_t DiagMessageHandlerTest::cSourceAddress;
        const uint16_t DiagMessageHandlerTest::cTargetAddress;
        const uint8_t DiagMessageHandlerTest::cNegativeResponseSid;

        TEST_F(DiagMessageHandlerTest, GetMessageMethod)
        {
            DoipLib::Message *_message{Handler.GetMessage()};

            ASSERT_NE(nullptr, _message);
            EXPECT_NE(nullptr, dynamic_cast<DoipLib::DiagMessage *>(_message));
        }

        TEST_F(DiagMessageHandlerTest, InvalidRequestType)
        {
            const DoipLib::VehicleIdRequest cRequest(cProtocolVersion);
            std::vector<uint8_t> _serializedResponse;

            EXPECT_FALSE(Handler.TryHandle(&cRequest, _serializedResponse));
        }

        TEST_F(DiagMessageHandlerTest, EmptyUserData)
        {
            std::vector<uint8_t> _udsResponse;
            EXPECT_FALSE(TryGetUdsResponse({}, _udsResponse));
        }

        TEST_F(DiagMessageHandlerTest, UnsupportedService)
        {
            const uint8_t cDiagnosticSessionControlSid{0x10};
            const uint8_t cServiceNotSupportedNrc{0x11};
            const std::vector<uint8_t> cExpectedResponse{
                cNegativeResponseSid, cDiagnosticSessionControlSid, cServiceNotSupportedNrc};

            std::vector<uint8_t> _udsResponse;
            EXPECT_TRUE(TryGetUdsResponse({cDiagnosticSessionControlSid, 0x01}, _udsResponse));
            EXPECT_EQ(cExpectedResponse, _udsResponse);
        }

        TEST_F(DiagMessageHandlerTest, UnsupportedDataIdentifier)
        {
            const uint8_t cReadDataByIdentifierSid{0x22};
            const uint8_t cRequestOutOfRangeNrc{0x31};
            const std::vector<uint8_t> cExpectedResponse{
                cNegativeResponseSid, cReadDataByIdentifierSid, cRequestOutOfRangeNrc};

            std::vector<uint8_t> _udsResponse;
            EXPECT_TRUE(TryGetUdsResponse({cReadDataByIdentifierSid, 0xf5, 0xff}, _udsResponse));
            EXPECT_EQ(cExpectedResponse, _udsResponse);
        }
    }
}
