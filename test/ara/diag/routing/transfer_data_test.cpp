#include "./testable_uds_service.h"
#include "../../../../src/ara/diag/routing/transfer_data.h"

namespace ara
{
    namespace diag
    {
        namespace routing
        {
            class TransferDataTest : public TestableUdsService
            {
            private:
                static core::InstanceSpecifier mSpecifier;

            protected:
                TransferData Service;

            public:
                TransferDataTest() : Service{mSpecifier}
                {
                }
            };

            core::InstanceSpecifier TransferDataTest::mSpecifier{"Instance0"};

            TEST_F(TransferDataTest, ValidTransferConfiguration)
            {
                const TransferDirection cTransferDirection{TransferDirection::kDownload};
                const size_t cMemoryAddress{512};
                const size_t cMemorySize{64};

                EXPECT_TRUE(
                    Service.TrySetTransferConfiguration(
                        cTransferDirection, cMemoryAddress, cMemorySize));

                EXPECT_FALSE(
                    Service.TrySetTransferConfiguration(
                        cTransferDirection, cMemoryAddress, cMemorySize));
            }

            TEST_F(TransferDataTest, InvalidTransferConfiguration)
            {
                const TransferDirection cValidTransferDirection{TransferDirection::kDownload};
                const size_t cValidMemoryAddress{512};
                const size_t cValidMemorySize{64};

                const TransferDirection cInvalidTransferDirection{TransferDirection::kNone};
                const size_t cInvalidMemoryAddress{2048};
                const size_t cInvalidMemorySize{1024};
                const size_t cZeroMemorySize{0};

                EXPECT_FALSE(
                    Service.TrySetTransferConfiguration(
                        cInvalidTransferDirection, cValidMemoryAddress, cValidMemorySize));

                EXPECT_FALSE(
                    Service.TrySetTransferConfiguration(
                        cValidTransferDirection, cInvalidMemoryAddress, cValidMemorySize));

                EXPECT_FALSE(
                    Service.TrySetTransferConfiguration(
                        cValidTransferDirection, cValidMemoryAddress, cZeroMemorySize));

                EXPECT_FALSE(
                    Service.TrySetTransferConfiguration(
                        cValidTransferDirection, cValidMemoryAddress, cInvalidMemorySize));
            }

            TEST_F(TransferDataTest, TransferConfigurationReset)
            {
                const TransferDirection cTransferDirection{TransferDirection::kDownload};
                const size_t cMemoryAddress{512};
                const size_t cMemorySize{64};

                EXPECT_FALSE(Service.TryResetTransferConfiguration());

                Service.TrySetTransferConfiguration(
                    cTransferDirection, cMemoryAddress, cMemorySize);

                EXPECT_TRUE(Service.TryResetTransferConfiguration());
            }

            TEST_F(TransferDataTest, IncorrectMessageLengthScenario)
            {
                const uint8_t cExpectedNrc{0x13};

                uint8_t _actualNrc;
                std::vector<uint8_t> _requestData{Service.GetSid()};
                bool _hasNrc{TryGetNrc(&Service, _requestData, _actualNrc)};

                EXPECT_TRUE(_hasNrc);
                EXPECT_EQ(cExpectedNrc, _actualNrc);
            }

            TEST_F(TransferDataTest, RequestSequenceErrorScenario)
            {
                const uint8_t cExpectedNrc{0x24};
                const uint8_t cBlockSequenceCounter{1};

                uint8_t _actualNrc;
                std::vector<uint8_t> _requestData{Service.GetSid(), cBlockSequenceCounter};
                bool _hasNrc{TryGetNrc(&Service, _requestData, _actualNrc)};

                EXPECT_TRUE(_hasNrc);
                EXPECT_EQ(cExpectedNrc, _actualNrc);
            }

            TEST_F(TransferDataTest, WrongBlockSequenceCounterScenario)
            {
                const TransferDirection cTransferDirection{TransferDirection::kDownload};
                const size_t cMemoryAddress{512};
                const size_t cMemorySize{64};
                const uint8_t cExpectedNrc{0x73};
                const uint8_t cSmallerBlockSequenceCounter{0};
                const uint8_t cGreaterBlockSequenceCounter{2};

                Service.TrySetTransferConfiguration(
                    cTransferDirection, cMemoryAddress, cMemorySize);

                uint8_t _actualNrc;
                std::vector<uint8_t> _requestData{Service.GetSid(), cSmallerBlockSequenceCounter};
                bool _hasNrc{TryGetNrc(&Service, _requestData, _actualNrc)};

                EXPECT_TRUE(_hasNrc);
                EXPECT_EQ(cExpectedNrc, _actualNrc);

                _requestData = {Service.GetSid(), cGreaterBlockSequenceCounter};
                _hasNrc = TryGetNrc(&Service, _requestData, _actualNrc);

                EXPECT_TRUE(_hasNrc);
                EXPECT_EQ(cExpectedNrc, _actualNrc);
            }

            class TransferDataMessageTest : public TransferDataTest
            {
            protected:
                const uint8_t cPositiveResponseSid{0x76};

                std::vector<uint8_t> Send(const std::vector<uint8_t> &requestData)
                {
                    CancellationHandler _cancellationHandler(false);
                    std::future<OperationOutput> _responseFuture{
                        Service.HandleMessage(
                            requestData, GeneralMetaInfo, std::move(_cancellationHandler))};
                    OperationOutput _response{_responseFuture.get()};

                    return _response.responseData;
                }
            };

            TEST_F(TransferDataMessageTest, DownloadThenUploadScenario)
            {
                const size_t cMemoryAddress{512};
                const size_t cMemorySize{4};
                const uint8_t cBlockSequenceCounter{1};
                const std::vector<uint8_t> cExpectedDownloadResponse{
                    cPositiveResponseSid, cBlockSequenceCounter};
                const std::vector<uint8_t> cExpectedUploadResponse{
                    cPositiveResponseSid, cBlockSequenceCounter, 0xde, 0xad, 0xbe, 0xef};

                EXPECT_TRUE(
                    Service.TrySetTransferConfiguration(
                        TransferDirection::kDownload, cMemoryAddress, cMemorySize));

                std::vector<uint8_t> _downloadRequestData{
                    Service.GetSid(), cBlockSequenceCounter, 0xde, 0xad, 0xbe, 0xef};
                EXPECT_EQ(cExpectedDownloadResponse, Send(_downloadRequestData));

                EXPECT_TRUE(Service.TryResetTransferConfiguration());
                EXPECT_TRUE(
                    Service.TrySetTransferConfiguration(
                        TransferDirection::kUpload, cMemoryAddress, cMemorySize));

                std::vector<uint8_t> _uploadRequestData{
                    Service.GetSid(), cBlockSequenceCounter};
                EXPECT_EQ(cExpectedUploadResponse, Send(_uploadRequestData));
            }

            TEST_F(TransferDataMessageTest, BlockSequenceCounterScenario)
            {
                const size_t cMemoryAddress{0};
                const size_t cMemorySize{1};
                const uint8_t cData{0xaa};
                const std::vector<uint8_t> cWrongCounterResponse{0x7f, 0x36, 0x73};

                Service.TrySetTransferConfiguration(
                    TransferDirection::kDownload, cMemoryAddress, cMemorySize);

                const std::vector<uint8_t> cAcceptedCounters{1, 1, 2, 3, 3};
                for (uint8_t _counter : cAcceptedCounters)
                {
                    const std::vector<uint8_t> cExpectedResponse{
                        cPositiveResponseSid, _counter};
                    std::vector<uint8_t> _requestData{Service.GetSid(), _counter, cData};

                    EXPECT_EQ(cExpectedResponse, Send(_requestData));
                }

                // Neither the current counter (3) nor the next one (4)
                std::vector<uint8_t> _skippedRequestData{Service.GetSid(), 5, cData};
                EXPECT_EQ(cWrongCounterResponse, Send(_skippedRequestData));

                std::vector<uint8_t> _previousRequestData{Service.GetSid(), 2, cData};
                EXPECT_EQ(cWrongCounterResponse, Send(_previousRequestData));
            }

            TEST_F(TransferDataMessageTest, ResetRestoresBlockSequenceCounter)
            {
                const size_t cMemoryAddress{0};
                const size_t cMemorySize{1};
                const uint8_t cData{0xaa};
                const std::vector<uint8_t> cWrongCounterResponse{0x7f, 0x36, 0x73};
                const std::vector<uint8_t> cExpectedResponse{cPositiveResponseSid, 1};

                Service.TrySetTransferConfiguration(
                    TransferDirection::kDownload, cMemoryAddress, cMemorySize);

                std::vector<uint8_t> _firstRequestData{Service.GetSid(), 1, cData};
                std::vector<uint8_t> _secondRequestData{Service.GetSid(), 2, cData};
                Send(_firstRequestData);
                Send(_secondRequestData);

                Service.TryResetTransferConfiguration();
                Service.TrySetTransferConfiguration(
                    TransferDirection::kDownload, cMemoryAddress, cMemorySize);

                EXPECT_EQ(cWrongCounterResponse, Send(_secondRequestData));
                EXPECT_EQ(cExpectedResponse, Send(_firstRequestData));
            }

            // BUG (AE-30): TransferData::tryTransfer copies the configured memory size from the request
            // without checking the transfer request parameter record length, so a download block
            // shorter than the configured memory size is read out of bounds instead of being rejected.
            TEST_F(TransferDataMessageTest, DISABLED_BugShortDownloadRecordIsRejected)
            {
                const size_t cMemoryAddress{512};
                const size_t cMemorySize{64};
                const uint8_t cBlockSequenceCounter{1};
                const uint8_t cData{0xaa};
                const std::vector<uint8_t> cExpectedResponse{0x7f, 0x36, 0x13};

                Service.TrySetTransferConfiguration(
                    TransferDirection::kDownload, cMemoryAddress, cMemorySize);

                std::vector<uint8_t> _requestData{
                    Service.GetSid(), cBlockSequenceCounter, cData};

                EXPECT_EQ(cExpectedResponse, Send(_requestData));
            }
        }
    }
}