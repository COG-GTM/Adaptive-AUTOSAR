#include <gtest/gtest.h>
#include <doiplib/diag_message.h>
#include <doiplib/vehicle_id_request.h>
#include <doiplib/vehicle_id_response.h>
#include "../../../src/application/doip/vehicle_id_request_handler.h"

namespace application
{
    namespace doip
    {
        class VehicleIdRequestHandlerTest : public testing::Test
        {
        protected:
            static const uint8_t cProtocolVersion{2};
            static const uint16_t cLogicalAddress{0x0e80};
            const std::string cVin{"YV1ABCDEFGH123456"};
            VehicleIdRequestHandler Handler;

            VehicleIdRequestHandlerTest() : Handler(
                                                cProtocolVersion,
                                                std::string(cVin),
                                                cLogicalAddress,
                                                0x060504030201,
                                                0x0f0e0d0c0b0a)
            {
            }
        };

        const uint8_t VehicleIdRequestHandlerTest::cProtocolVersion;
        const uint16_t VehicleIdRequestHandlerTest::cLogicalAddress;

        TEST_F(VehicleIdRequestHandlerTest, GetMessage)
        {
            DoipLib::Message *_message{Handler.GetMessage()};
            ASSERT_NE(nullptr, _message);
            EXPECT_NE(nullptr, dynamic_cast<DoipLib::VehicleIdRequest *>(_message));
        }

        TEST_F(VehicleIdRequestHandlerTest, HandleVehicleIdRequest)
        {
            const DoipLib::VehicleIdRequest cRequest(cProtocolVersion);
            std::vector<uint8_t> _serializedResponse;
            ASSERT_TRUE(Handler.TryHandle(&cRequest, _serializedResponse));

            DoipLib::PayloadType _payloadType;
            ASSERT_TRUE(
                DoipLib::Message::TryExtractPayloadType(_serializedResponse, _payloadType));
            EXPECT_EQ(DoipLib::PayloadType::VehicleAnnoucementIdResponse, _payloadType);

            DoipLib::VehicleIdResponse _response;
            DoipLib::GenericNackType _nack;
            ASSERT_TRUE(_response.TryDeserialize(_serializedResponse, _nack));
            EXPECT_EQ(cVin, _response.GetVin());
            EXPECT_EQ(cLogicalAddress, _response.GetLogicalAddress());
            EXPECT_EQ(0x00, _response.GetFurtherAction());
        }

        TEST_F(VehicleIdRequestHandlerTest, EidAndGidConversion)
        {
            const DoipLib::VehicleIdRequest cRequest(cProtocolVersion);
            std::vector<uint8_t> _serializedResponse;
            ASSERT_TRUE(Handler.TryHandle(&cRequest, _serializedResponse));

            DoipLib::VehicleIdResponse _response;
            DoipLib::GenericNackType _nack;
            ASSERT_TRUE(_response.TryDeserialize(_serializedResponse, _nack));

            const std::array<uint8_t, 6> cExpectedEid{0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
            const std::array<uint8_t, 6> cExpectedGid{0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
            EXPECT_EQ(cExpectedEid, _response.GetEid());
            EXPECT_EQ(cExpectedGid, _response.GetGid());
        }

        TEST_F(VehicleIdRequestHandlerTest, RejectOtherMessages)
        {
            const DoipLib::DiagMessage cRequest(cProtocolVersion, 1, 2, {0x22});
            std::vector<uint8_t> _response;
            EXPECT_FALSE(Handler.TryHandle(&cRequest, _response));
            EXPECT_TRUE(_response.empty());
        }
    }
}
