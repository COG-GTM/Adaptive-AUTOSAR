#include <gtest/gtest.h>
#include "./loopback_doip_server.h"

namespace application
{
    namespace doip
    {
        class DoipServerTest : public testing::Test
        {
        protected:
            static const uint16_t cPort{18081};
            static const std::string cVin;

            LoopbackDoipServer Server;
            LoopbackDoipClient Client;

            DoipServerTest() : Server(cPort, cVin),
                               Client(cPort)
            {
            }
        };

        const uint16_t DoipServerTest::cPort;
        const std::string DoipServerTest::cVin{"YV1LOOPBACKTEST01"};

        TEST_F(DoipServerTest, ReadVinOverDoip)
        {
            // DoIP header (diagnostic message 0x8001, 7 bytes payload)
            // + SA 0x0E80 + TA 0x0001 + UDS 22 F1 90
            const std::vector<uint8_t> cRequest{
                0x02, 0xfd, 0x80, 0x01, 0x00, 0x00, 0x00, 0x07,
                0x0e, 0x80, 0x00, 0x01,
                0x22, 0xf1, 0x90};

            // DoIP header (diagnostic message positive ACK 0x8002, 25 bytes payload)
            // + request addresses echoed as-is by the DiagMessageHandler
            // + ACK code 0x00 + UDS 62 F1 90 <VIN>
            std::vector<uint8_t> _expectedResponse{
                0x02, 0xfd, 0x80, 0x02, 0x00, 0x00, 0x00, 0x19,
                0x0e, 0x80, 0x00, 0x01, 0x00,
                0x62, 0xf1, 0x90};
            _expectedResponse.insert(
                _expectedResponse.end(), cVin.cbegin(), cVin.cend());

            std::vector<uint8_t> _actualResponse;
            const bool cSucceed{
                Client.TryExchange(cRequest, _actualResponse)};

            ASSERT_TRUE(cSucceed);
            EXPECT_EQ(_expectedResponse, _actualResponse);
        }

        TEST_F(DoipServerTest, ReadUnknownDidOverDoip)
        {
            const std::vector<uint8_t> cRequest{
                0x02, 0xfd, 0x80, 0x01, 0x00, 0x00, 0x00, 0x07,
                0x0e, 0x80, 0x00, 0x01,
                0x22, 0xf1, 0x91};

            // UDS negative response 7F 22 31 (request out of range)
            const std::vector<uint8_t> cExpectedResponse{
                0x02, 0xfd, 0x80, 0x02, 0x00, 0x00, 0x00, 0x08,
                0x0e, 0x80, 0x00, 0x01, 0x00,
                0x7f, 0x22, 0x31};

            std::vector<uint8_t> _actualResponse;
            const bool cSucceed{
                Client.TryExchange(cRequest, _actualResponse)};

            ASSERT_TRUE(cSucceed);
            EXPECT_EQ(cExpectedResponse, _actualResponse);
        }

        TEST_F(DoipServerTest, ReadUnavailableDidOverDoip)
        {
            // Average speed DID which requires the (unreachable) RESTful resource
            const std::vector<uint8_t> cRequest{
                0x02, 0xfd, 0x80, 0x01, 0x00, 0x00, 0x00, 0x07,
                0x0e, 0x80, 0x00, 0x01,
                0x22, 0xf5, 0x0d};

            // UDS negative response 7F 22 22 (conditions not correct)
            const std::vector<uint8_t> cExpectedResponse{
                0x02, 0xfd, 0x80, 0x02, 0x00, 0x00, 0x00, 0x08,
                0x0e, 0x80, 0x00, 0x01, 0x00,
                0x7f, 0x22, 0x22};

            std::vector<uint8_t> _actualResponse;
            const bool cSucceed{
                Client.TryExchange(cRequest, _actualResponse)};

            ASSERT_TRUE(cSucceed);
            EXPECT_EQ(cExpectedResponse, _actualResponse);
        }

        TEST_F(DoipServerTest, ConsecutiveReadsOverSameConnection)
        {
            const size_t cUdsResponseOffset{13};
            const std::vector<uint8_t> cVinRequest{
                0x02, 0xfd, 0x80, 0x01, 0x00, 0x00, 0x00, 0x07,
                0x0e, 0x80, 0x00, 0x01,
                0x22, 0xf1, 0x90};
            const std::vector<uint8_t> cUnknownDidRequest{
                0x02, 0xfd, 0x80, 0x01, 0x00, 0x00, 0x00, 0x07,
                0x0e, 0x80, 0x00, 0x01,
                0x22, 0xf1, 0x91};

            std::vector<uint8_t> _vinResponse;
            ASSERT_TRUE(Client.TryExchange(cVinRequest, _vinResponse));
            EXPECT_EQ(0x62, _vinResponse.at(cUdsResponseOffset));

            std::vector<uint8_t> _unknownDidResponse;
            ASSERT_TRUE(
                Client.TryExchange(cUnknownDidRequest, _unknownDidResponse));
            EXPECT_EQ(0x7f, _unknownDidResponse.at(cUdsResponseOffset));
        }
    }
}
