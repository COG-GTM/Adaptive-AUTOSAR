#include <gtest/gtest.h>
#include "../../../src/application/helper/read_data_by_identifier.h"

namespace application
{
    namespace helper
    {
        class ReadDataByIdentifierTest : public testing::Test
        {
        protected:
            static const uint8_t cSid{0x22};
            static const uint8_t cNegativeResponseSid{0x7f};
            static const std::string cVin;
            /// @note Nothing listens on the discard port, so every RESTful request fails
            static const std::string cUnreachableResourcesUrl;

            CurlWrapper Curl;
            ara::diag::MetaInfo GeneralMetaInfo;

            ReadDataByIdentifierTest() : Curl("dummy_api_key", "dummy_bearer_token"),
                                         GeneralMetaInfo(ara::diag::Context::kDoIP)
            {
            }

            std::vector<uint8_t> GetResponse(
                ReadDataByIdentifier &service,
                const std::vector<uint8_t> &requestData)
            {
                ara::diag::CancellationHandler _cancellationHandler(false);

                std::future<ara::diag::OperationOutput> _responseFuture{
                    service.HandleMessage(
                        requestData,
                        GeneralMetaInfo,
                        std::move(_cancellationHandler))};

                ara::diag::OperationOutput _response{_responseFuture.get()};

                return _response.responseData;
            }
        };

        const uint8_t ReadDataByIdentifierTest::cSid;
        const uint8_t ReadDataByIdentifierTest::cNegativeResponseSid;
        const std::string ReadDataByIdentifierTest::cVin{"YV1LOOPBACKTEST01"};
        const std::string ReadDataByIdentifierTest::cUnreachableResourcesUrl{
            "http://127.0.0.1:9/resources"};

        TEST_F(ReadDataByIdentifierTest, Constructor)
        {
            ReadDataByIdentifier _service(&Curl, cUnreachableResourcesUrl, cVin);

            EXPECT_EQ(cSid, _service.GetSid());
        }

        TEST_F(ReadDataByIdentifierTest, VinPositiveResponse)
        {
            ReadDataByIdentifier _service(&Curl, cUnreachableResourcesUrl, cVin);
            const std::vector<uint8_t> cRequestData{cSid, 0xf1, 0x90};

            // 0x62 (positive response SID) + DID 0xF190 + 17 ASCII VIN characters
            std::vector<uint8_t> _expectedResult{0x62, 0xf1, 0x90};
            _expectedResult.insert(
                _expectedResult.end(), cVin.cbegin(), cVin.cend());

            const std::vector<uint8_t> cActualResult{
                GetResponse(_service, cRequestData)};

            EXPECT_EQ(_expectedResult, cActualResult);
        }

        TEST_F(ReadDataByIdentifierTest, VinRepeatedRead)
        {
            ReadDataByIdentifier _service(&Curl, cUnreachableResourcesUrl, cVin);
            const std::vector<uint8_t> cRequestData{cSid, 0xf1, 0x90};

            const std::vector<uint8_t> cFirstResult{
                GetResponse(_service, cRequestData)};
            const std::vector<uint8_t> cSecondResult{
                GetResponse(_service, cRequestData)};

            EXPECT_EQ(cFirstResult, cSecondResult);
        }

        TEST_F(ReadDataByIdentifierTest, VinConditionsNotCorrect)
        {
            const std::string cEmptyVin;
            const std::string cShortVin{"YV1SHORT"};
            const std::vector<uint8_t> cRequestData{cSid, 0xf1, 0x90};
            const std::vector<uint8_t> cExpectedResult{
                cNegativeResponseSid, cSid, 0x22};

            ReadDataByIdentifier _emptyVinService(
                &Curl, cUnreachableResourcesUrl, cEmptyVin);
            EXPECT_EQ(
                cExpectedResult, GetResponse(_emptyVinService, cRequestData));

            ReadDataByIdentifier _shortVinService(
                &Curl, cUnreachableResourcesUrl, cShortVin);
            EXPECT_EQ(
                cExpectedResult, GetResponse(_shortVinService, cRequestData));
        }

        TEST_F(ReadDataByIdentifierTest, UnknownDidRequestOutOfRange)
        {
            ReadDataByIdentifier _service(&Curl, cUnreachableResourcesUrl, cVin);
            const std::vector<uint8_t> cRequestData{cSid, 0xf1, 0x91};
            const std::vector<uint8_t> cExpectedResult{
                cNegativeResponseSid, cSid, 0x31};

            const std::vector<uint8_t> cActualResult{
                GetResponse(_service, cRequestData)};

            EXPECT_EQ(cExpectedResult, cActualResult);
        }

        TEST_F(ReadDataByIdentifierTest, ExistingDidConditionsNotCorrect)
        {
            ReadDataByIdentifier _service(&Curl, cUnreachableResourcesUrl, cVin);
            // Average speed DID which requires the (unreachable) RESTful resource
            const std::vector<uint8_t> cRequestData{cSid, 0xf5, 0x0d};
            const std::vector<uint8_t> cExpectedResult{
                cNegativeResponseSid, cSid, 0x22};

            const std::vector<uint8_t> cActualResult{
                GetResponse(_service, cRequestData)};

            EXPECT_EQ(cExpectedResult, cActualResult);
        }
    }
}
