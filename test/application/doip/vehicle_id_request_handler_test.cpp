#include <gtest/gtest.h>
#include <doiplib/diag_message.h>
#include <doiplib/doip_controller.h>
#include "../../../src/application/doip/vehicle_id_request_handler.h"

namespace application
{
    namespace doip
    {
        class VehicleIdRequestHandlerTest : public testing::Test
        {
        protected:
            static const uint8_t cProtocolVersion{0x02};
            static const uint16_t cLogicalAddress{0x0e80};
            static const uint64_t cEid{0x0000000000ab};
            static const uint64_t cGid{0x0000000000cd};
            const std::string cVin{"WVWZZZ1JZXW000001"};

            VehicleIdRequestHandler Handler;

            VehicleIdRequestHandlerTest() : Handler(cProtocolVersion, std::string(cVin), cLogicalAddress, cEid, cGid)
            {
            }
        };

        const uint8_t VehicleIdRequestHandlerTest::cProtocolVersion;
        const uint16_t VehicleIdRequestHandlerTest::cLogicalAddress;
        const uint64_t VehicleIdRequestHandlerTest::cEid;
        const uint64_t VehicleIdRequestHandlerTest::cGid;

        TEST_F(VehicleIdRequestHandlerTest, GetMessageMethod)
        {
            DoipLib::Message *_message{Handler.GetMessage()};

            ASSERT_NE(nullptr, _message);
            EXPECT_NE(nullptr, dynamic_cast<DoipLib::VehicleIdRequest *>(_message));
        }

        TEST_F(VehicleIdRequestHandlerTest, ValidRequest)
        {
            const uint8_t cFurtherAction{0x00};
            const std::array<uint8_t, 6> cExpectedEid{0xab, 0x00, 0x00, 0x00, 0x00, 0x00};
            const std::array<uint8_t, 6> cExpectedGid{0xcd, 0x00, 0x00, 0x00, 0x00, 0x00};

            const DoipLib::VehicleIdRequest cRequest(cProtocolVersion);
            std::vector<uint8_t> _serializedResponse;

            EXPECT_TRUE(Handler.TryHandle(&cRequest, _serializedResponse));

            DoipLib::VehicleIdResponse _response;
            DoipLib::GenericNackType _nackCode;
            ASSERT_TRUE(_response.TryDeserialize(_serializedResponse, _nackCode));

            EXPECT_EQ(cVin, _response.GetVin());
            EXPECT_EQ(cLogicalAddress, _response.GetLogicalAddress());
            EXPECT_EQ(cExpectedEid, _response.GetEid());
            EXPECT_EQ(cExpectedGid, _response.GetGid());
            EXPECT_EQ(cFurtherAction, _response.GetFurtherAction());
        }

        TEST_F(VehicleIdRequestHandlerTest, InvalidRequest)
        {
            const uint16_t cSourceAddress{0x0001};
            const uint16_t cTargetAddress{0x0002};
            const std::vector<uint8_t> cUserData{0x22, 0xf5, 0x0d};
            const DoipLib::DiagMessage cRequest(
                cProtocolVersion, cSourceAddress, cTargetAddress, cUserData);

            std::vector<uint8_t> _serializedResponse;

            EXPECT_FALSE(Handler.TryHandle(&cRequest, _serializedResponse));
            EXPECT_TRUE(_serializedResponse.empty());
        }

        TEST_F(VehicleIdRequestHandlerTest, ControllerIntegration)
        {
            DoipLib::ControllerConfig _config;
            _config.protocolVersion = cProtocolVersion;
            _config.doipMaxRequestBytes = 64;
            _config.doIPInitialVehicleAnnouncementTime = std::chrono::seconds(0);
            _config.doIPVehicleAnnouncementCount = 3;
            _config.doIPVehicleAnnouncementInterval = std::chrono::seconds(1);

            DoipLib::DoipController _controller(std::move(_config));
            _controller.Register(DoipLib::PayloadType::VehicleIdRequest, &Handler);

            const DoipLib::VehicleIdRequest cRequest(cProtocolVersion);
            std::vector<uint8_t> _serializedRequest;
            cRequest.Serialize(_serializedRequest);

            std::vector<uint8_t> _serializedResponse;
            EXPECT_TRUE(_controller.TryHandle(_serializedRequest, _serializedResponse));

            DoipLib::VehicleIdResponse _response;
            DoipLib::GenericNackType _nackCode;
            ASSERT_TRUE(_response.TryDeserialize(_serializedResponse, _nackCode));
            EXPECT_EQ(cLogicalAddress, _response.GetLogicalAddress());
        }
    }
}
