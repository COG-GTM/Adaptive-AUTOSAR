#include <gtest/gtest.h>
#include <doiplib/diag_message.h>
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
            const std::string cVin{"WAUZZZ8V9KA000001"};

            static bool TryGetResponse(
                uint64_t eid, uint64_t gid, DoipLib::VehicleIdResponse &response)
            {
                std::string _vin{"WAUZZZ8V9KA000001"};
                const VehicleIdRequestHandler cHandler(
                    cProtocolVersion, std::move(_vin), cLogicalAddress, eid, gid);
                const DoipLib::VehicleIdRequest cRequest(cProtocolVersion);

                std::vector<uint8_t> _serializedResponse;
                const bool cHandled{
                    cHandler.TryHandle(&cRequest, _serializedResponse)};

                DoipLib::GenericNackType _nackCode;
                const bool cResult{
                    cHandled &&
                    response.TryDeserialize(_serializedResponse, _nackCode)};

                return cResult;
            }
        };

        const uint8_t VehicleIdRequestHandlerTest::cProtocolVersion;
        const uint16_t VehicleIdRequestHandlerTest::cLogicalAddress;

        TEST_F(VehicleIdRequestHandlerTest, GetMessageReturnsVehicleIdRequest)
        {
            std::string _vin{cVin};
            VehicleIdRequestHandler _handler(
                cProtocolVersion, std::move(_vin), cLogicalAddress, 0, 0);

            DoipLib::Message *_message{_handler.GetMessage()};

            ASSERT_NE(nullptr, _message);
            EXPECT_NE(
                nullptr, dynamic_cast<DoipLib::VehicleIdRequest *>(_message));
            // The same request instance is reused for all the calls
            EXPECT_EQ(_message, _handler.GetMessage());
        }

        TEST_F(VehicleIdRequestHandlerTest, HandleVehicleIdRequest)
        {
            DoipLib::VehicleIdResponse _response;
            ASSERT_TRUE(TryGetResponse(0, 0, _response));

            const uint8_t cExpectedFurtherAction{0x00};
            EXPECT_EQ(cVin, _response.GetVin());
            EXPECT_EQ(cLogicalAddress, _response.GetLogicalAddress());
            EXPECT_EQ(cExpectedFurtherAction, _response.GetFurtherAction());
        }

        TEST_F(VehicleIdRequestHandlerTest, ResponseHeader)
        {
            std::string _vin{cVin};
            const VehicleIdRequestHandler cHandler(
                cProtocolVersion, std::move(_vin), cLogicalAddress, 0, 0);
            const DoipLib::VehicleIdRequest cRequest(cProtocolVersion);

            std::vector<uint8_t> _serializedResponse;
            ASSERT_TRUE(cHandler.TryHandle(&cRequest, _serializedResponse));

            // Protocol version, inverse protocol version and vehicle announcement payload type (0x0004)
            const std::vector<uint8_t> cExpectedHeader{0x02, 0xfd, 0x00, 0x04};
            ASSERT_GE(_serializedResponse.size(), cExpectedHeader.size());
            const std::vector<uint8_t> cActualHeader(
                _serializedResponse.cbegin(),
                _serializedResponse.cbegin() + cExpectedHeader.size());
            EXPECT_EQ(cExpectedHeader, cActualHeader);

            DoipLib::PayloadType _payloadType;
            ASSERT_TRUE(
                DoipLib::Message::TryExtractPayloadType(
                    _serializedResponse, _payloadType));
            EXPECT_EQ(
                DoipLib::PayloadType::VehicleAnnoucementIdResponse, _payloadType);
        }

        TEST_F(VehicleIdRequestHandlerTest, HandleIsRepeatable)
        {
            std::string _vin{cVin};
            const VehicleIdRequestHandler cHandler(
                cProtocolVersion, std::move(_vin), cLogicalAddress, 0x12, 0x34);
            const DoipLib::VehicleIdRequest cRequest(cProtocolVersion);

            std::vector<uint8_t> _firstResponse;
            std::vector<uint8_t> _secondResponse;
            EXPECT_TRUE(cHandler.TryHandle(&cRequest, _firstResponse));
            EXPECT_TRUE(cHandler.TryHandle(&cRequest, _secondResponse));

            EXPECT_EQ(_firstResponse, _secondResponse);
        }

        TEST_F(VehicleIdRequestHandlerTest, RejectNonVehicleIdRequest)
        {
            std::string _vin{cVin};
            const VehicleIdRequestHandler cHandler(
                cProtocolVersion, std::move(_vin), cLogicalAddress, 0, 0);
            const DoipLib::DiagMessage cRequest(
                cProtocolVersion, 0x0001, 0x0002, std::vector<uint8_t>{0x22});

            std::vector<uint8_t> _serializedResponse;
            EXPECT_FALSE(cHandler.TryHandle(&cRequest, _serializedResponse));
            EXPECT_TRUE(_serializedResponse.empty());
        }

        TEST_F(VehicleIdRequestHandlerTest, RejectNullRequest)
        {
            std::string _vin{cVin};
            const VehicleIdRequestHandler cHandler(
                cProtocolVersion, std::move(_vin), cLogicalAddress, 0, 0);

            std::vector<uint8_t> _serializedResponse;
            EXPECT_FALSE(cHandler.TryHandle(nullptr, _serializedResponse));
            EXPECT_TRUE(_serializedResponse.empty());
        }

        TEST_F(VehicleIdRequestHandlerTest, SingleOctetIds)
        {
            const uint64_t cEid{0xab};
            const uint64_t cGid{0xcd};
            const std::array<uint8_t, 6> cExpectedEid{0xab, 0x00, 0x00, 0x00, 0x00, 0x00};
            const std::array<uint8_t, 6> cExpectedGid{0xcd, 0x00, 0x00, 0x00, 0x00, 0x00};

            DoipLib::VehicleIdResponse _response;
            ASSERT_TRUE(TryGetResponse(cEid, cGid, _response));

            EXPECT_EQ(cExpectedEid, _response.GetEid());
            EXPECT_EQ(cExpectedGid, _response.GetGid());
        }

        /// @note BUG (production code, not fixed here): 'convertToMacAddress' shifts its
        ///       octet mask by 'i * 8' bits per iteration instead of 8 bits, so only the
        ///       octets 0 and 3 of an entity/group ID reach the vehicle ID response and
        ///       the other four octets are zeroed. Enable the test once the mask is fixed.
        TEST_F(VehicleIdRequestHandlerTest, DISABLED_BugMultiOctetIdsAreTruncated)
        {
            const uint64_t cEid{0x060504030201};
            const uint64_t cGid{0x0c0b0a090807};
            const std::array<uint8_t, 6> cExpectedEid{0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
            const std::array<uint8_t, 6> cExpectedGid{0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c};

            DoipLib::VehicleIdResponse _response;
            ASSERT_TRUE(TryGetResponse(cEid, cGid, _response));

            EXPECT_EQ(cExpectedEid, _response.GetEid());
            EXPECT_EQ(cExpectedGid, _response.GetGid());
        }

        /// @note Pins the current (buggy) conversion that 'DISABLED_BugMultiOctetIdsAreTruncated' documents,
        ///       so that a fix of the production code is noticed by the test suite.
        TEST_F(VehicleIdRequestHandlerTest, MultiOctetIdsCurrentBehaviour)
        {
            const uint64_t cEid{0x060504030201};
            const std::array<uint8_t, 6> cCurrentEid{0x01, 0x00, 0x00, 0x04, 0x00, 0x00};

            DoipLib::VehicleIdResponse _response;
            ASSERT_TRUE(TryGetResponse(cEid, cEid, _response));

            EXPECT_EQ(cCurrentEid, _response.GetEid());
            EXPECT_EQ(cCurrentEid, _response.GetGid());
        }
    }
}
