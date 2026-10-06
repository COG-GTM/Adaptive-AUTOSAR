#include <gtest/gtest.h>
#include "../../../src/ara/diag/conversation.h"
#include "../../../src/application/helper/read_data_by_identifier.h"
#include "./mock_http_server.h"
#include "./testable_fixture.h"

namespace application
{
    namespace helper
    {
        class ReadDataByIdentifierTest : public testing::Test
        {
        protected:
            static const uint8_t cSid{0x22};
            static const uint8_t cPositiveSid{0x62};
            static const uint8_t cNegativeSid{0x7f};
            static const uint8_t cConditionsNotCorrectNrc{0x22};
            static const uint8_t cRequestOutOfRangeNrc{0x31};

            fixture::MockHttpServer Server;
            CurlWrapper Curl;
            ReadDataByIdentifier Service;

            ReadDataByIdentifierTest() : Curl("api-key", "bearer-token"),
                                         Service(&Curl, Server.GetUrl("/resources"))
            {
            }

            void SetResource(const std::string &key, const std::string &value)
            {
                Server.SetResponse(
                    "/resources/" + key,
                    "{\"" + key + "\":{\"value\":\"" + value + "\",\"unit\":\"x\"}}");
            }

            std::vector<uint8_t> Handle(std::vector<uint8_t> request)
            {
                ara::diag::MetaInfo _metaInfo(ara::diag::Context::kDoIP);
                ara::diag::CancellationHandler _cancellationHandler(false);
                auto _future{
                    Service.HandleMessage(
                        request, _metaInfo, std::move(_cancellationHandler))};
                return _future.get().responseData;
            }
        };

        const uint8_t ReadDataByIdentifierTest::cSid;
        const uint8_t ReadDataByIdentifierTest::cPositiveSid;
        const uint8_t ReadDataByIdentifierTest::cNegativeSid;
        const uint8_t ReadDataByIdentifierTest::cConditionsNotCorrectNrc;
        const uint8_t ReadDataByIdentifierTest::cRequestOutOfRangeNrc;

        TEST_F(ReadDataByIdentifierTest, ServiceId)
        {
            EXPECT_EQ(cSid, Service.GetSid());
        }

        TEST_F(ReadDataByIdentifierTest, AverageSpeed)
        {
            SetResource("averageSpeed", "65");
            const std::vector<uint8_t> cExpected{cPositiveSid, 0xf5, 0x0d, 65};
            EXPECT_EQ(cExpected, Handle({cSid, 0xf5, 0x0d}));
            EXPECT_EQ(0, Server.GetLastRequest().find("GET /resources/averageSpeed "));
        }

        TEST_F(ReadDataByIdentifierTest, FuelAmount)
        {
            SetResource("fuelAmount", "40");
            const std::vector<uint8_t> cExpected{cPositiveSid, 0xf5, 0x2f, 102};
            EXPECT_EQ(cExpected, Handle({cSid, 0xf5, 0x2f}));
        }

        TEST_F(ReadDataByIdentifierTest, ExternalTemperature)
        {
            SetResource("externalTemp", "20");
            const std::vector<uint8_t> cExpected{cPositiveSid, 0xf5, 0x46, 60};
            EXPECT_EQ(cExpected, Handle({cSid, 0xf5, 0x46}));
        }

        TEST_F(ReadDataByIdentifierTest, NegativeExternalTemperature)
        {
            SetResource("externalTemp", "-10");
            const std::vector<uint8_t> cExpected{cPositiveSid, 0xf5, 0x46, 30};
            EXPECT_EQ(cExpected, Handle({cSid, 0xf5, 0x46}));
        }

        TEST_F(ReadDataByIdentifierTest, AverageFuelConsumption)
        {
            SetResource("averageFuelConsumption", "13.0");
            const std::vector<uint8_t> cExpected{cPositiveSid, 0xf5, 0x5e, 0x01, 0x04};
            EXPECT_EQ(cExpected, Handle({cSid, 0xf5, 0x5e}));
        }

        TEST_F(ReadDataByIdentifierTest, EngineCoolantTemperature)
        {
            SetResource("engineCoolantTemp", "90");
            const std::vector<uint8_t> cExpected{cPositiveSid, 0xf5, 0x05, 130};
            EXPECT_EQ(cExpected, Handle({cSid, 0xf5, 0x05}));
        }

        TEST_F(ReadDataByIdentifierTest, OdometerValue)
        {
            SetResource("odometer", "1000.5");
            const std::vector<uint8_t> cExpected{
                cPositiveSid, 0xf5, 0xa6, 0x00, 0x00, 0x27, 0x15};
            EXPECT_EQ(cExpected, Handle({cSid, 0xf5, 0xa6}));
        }

        TEST_F(ReadDataByIdentifierTest, UnsupportedDid)
        {
            const std::vector<uint8_t> cExpected{cNegativeSid, cSid, cRequestOutOfRangeNrc};
            EXPECT_EQ(cExpected, Handle({cSid, 0x12, 0x34}));
            EXPECT_EQ(0, Server.GetRequestCount());
        }

        TEST_F(ReadDataByIdentifierTest, UnreachableResource)
        {
            const uint16_t cUnusedPort{fixture::GetFreePort()};
            ReadDataByIdentifier _service(
                &Curl, "http://127.0.0.1:" + std::to_string(cUnusedPort) + "/resources");

            ara::diag::MetaInfo _metaInfo(ara::diag::Context::kDoIP);
            auto _future{
                _service.HandleMessage(
                    {cSid, 0xf5, 0x0d}, _metaInfo, ara::diag::CancellationHandler(false))};

            const std::vector<uint8_t> cExpected{cNegativeSid, cSid, cConditionsNotCorrectNrc};
            EXPECT_EQ(cExpected, _future.get().responseData);
        }

        TEST_F(ReadDataByIdentifierTest, InvalidJsonResponse)
        {
            Server.SetResponse("/resources/averageSpeed", "not a JSON document");
            const std::vector<uint8_t> cExpected{cNegativeSid, cSid, cConditionsNotCorrectNrc};
            EXPECT_EQ(cExpected, Handle({cSid, 0xf5, 0x0d}));
        }

        TEST_F(ReadDataByIdentifierTest, MissingResourceValue)
        {
            Server.SetResponse("/resources/averageSpeed", "{\"averageSpeed\":{}}");
            EXPECT_THROW(Handle({cSid, 0xf5, 0x0d}), std::invalid_argument);
        }

        TEST_F(ReadDataByIdentifierTest, TooShortRequest)
        {
            EXPECT_THROW(Handle({cSid, 0xf5}), std::out_of_range);
        }

        TEST_F(ReadDataByIdentifierTest, PositiveResponseIsCached)
        {
            SetResource("averageSpeed", "65");
            const std::vector<uint8_t> cExpected{cPositiveSid, 0xf5, 0x0d, 65};
            EXPECT_EQ(cExpected, Handle({cSid, 0xf5, 0x0d}));

            SetResource("averageSpeed", "70");
            EXPECT_EQ(cExpected, Handle({cSid, 0xf5, 0x0d}));
            EXPECT_EQ(1, Server.GetRequestCount());
        }

        TEST_F(ReadDataByIdentifierTest, NegativeResponseIsNotCached)
        {
            Server.SetResponse("/resources/averageSpeed", "invalid");
            const std::vector<uint8_t> cNegative{cNegativeSid, cSid, cConditionsNotCorrectNrc};
            EXPECT_EQ(cNegative, Handle({cSid, 0xf5, 0x0d}));

            SetResource("averageSpeed", "65");
            const std::vector<uint8_t> cPositive{cPositiveSid, 0xf5, 0x0d, 65};
            EXPECT_EQ(cPositive, Handle({cSid, 0xf5, 0x0d}));
            EXPECT_EQ(2, Server.GetRequestCount());
        }

        TEST_F(ReadDataByIdentifierTest, ConversationIsDeactivated)
        {
            SetResource("averageSpeed", "65");
            const size_t cActiveConversations{
                ara::diag::Conversation::GetCurrentActiveConversations().size()};
            const size_t cAllConversations{
                ara::diag::Conversation::GetAllConversations().size()};

            Handle({cSid, 0xf5, 0x0d});

            EXPECT_EQ(
                cAllConversations + 1,
                ara::diag::Conversation::GetAllConversations().size());
            EXPECT_EQ(
                cActiveConversations,
                ara::diag::Conversation::GetCurrentActiveConversations().size());
        }
    }
}
