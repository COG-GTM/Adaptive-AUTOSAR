#include "./routing/testable_uds_service.h"
#include "../../../src/ara/diag/download.h"
#include "../../../src/ara/diag/routing/nrc_exception.h"

namespace ara
{
    namespace diag
    {
        class DownloadServiceTest : public routing::TestableUdsService
        {
        private:
            static const core::InstanceSpecifier cSpecifier;
            const std::string cMaxNumberOfBlockLengthKey{"MaxNumberOfBlockLength"};

        protected:
            static const uint8_t cSid{0x34};
            const uint8_t cMaxNumberOfBlockLength{64};
            const uint8_t cDataFormatIdentifier{0x00};
            const uint8_t cAddressAndLengthFormatIdentifier{0x22};
            const uint8_t cRequestOutOfRangeNrc{0x31};
            const uint8_t cUploadDownloadNotAcceptedNrc{0x70};
            const uint8_t cIncorrectMessageLengthNrc{0x13};
            const uint8_t cPositiveResponseSidIncrement{0x40};

            routing::TransferData TransferData;
            DownloadService Service;

            DownloadServiceTest() : TransferData(cSpecifier),
                        Service(cSpecifier, ReentrancyType::kNot, TransferData)
            {
                const std::string cMaxNumberOfBlockLengthStr{
                    std::to_string(cMaxNumberOfBlockLength)};
                GeneralMetaInfo.SetValue(
                    cMaxNumberOfBlockLengthKey, cMaxNumberOfBlockLengthStr);
            }

            std::future<void> Request(std::vector<uint8_t> memoryAddressAndSize)
            {
                CancellationHandler _cancellationHandler(false);

                return Service.RequestDownload(
                    cDataFormatIdentifier,
                    cAddressAndLengthFormatIdentifier,
                    memoryAddressAndSize,
                    GeneralMetaInfo,
                    std::move(_cancellationHandler));
            }
        };

        const core::InstanceSpecifier DownloadServiceTest::cSpecifier{"Instance0"};
        const uint8_t DownloadServiceTest::cSid;

        TEST_F(DownloadServiceTest, Constructor)
        {
            EXPECT_EQ(cSid, Service.GetSid());
        }

        TEST_F(DownloadServiceTest, RequestDownloadMethod)
        {
            const std::vector<uint8_t> cMemoryAddressAndSize{0x01, 0x00, 0x00, 0x40};

            std::future<void> _result{Request(cMemoryAddressAndSize)};
            EXPECT_NO_THROW(_result.get());
        }

        TEST_F(DownloadServiceTest, DirectionIsConfigured)
        {
            const std::vector<uint8_t> cMemoryAddressAndSize{0x01, 0x00, 0x00, 0x40};
            const size_t cMemoryAddress{256};
            const size_t cMemorySize{64};

            Request(cMemoryAddressAndSize).get();

            // The transfer direction is already occupied by the service request.
            EXPECT_FALSE(
                TransferData.TrySetTransferConfiguration(
                    routing::TransferDirection::kUpload, cMemoryAddress, cMemorySize));

            EXPECT_TRUE(TransferData.TryResetTransferConfiguration());
            EXPECT_TRUE(
                TransferData.TrySetTransferConfiguration(
                    routing::TransferDirection::kUpload, cMemoryAddress, cMemorySize));
        }

        TEST_F(DownloadServiceTest, InvalidLengthFormat)
        {
            const std::vector<uint8_t> cInvalidMemoryAddressAndSize{0x01, 0x00, 0x40};

            std::future<void> _result{Request(cInvalidMemoryAddressAndSize)};

            try
            {
                _result.get();
                FAIL() << "Expected an NRC exception.";
            }
            catch (const routing::NrcExecption &ex)
            {
                EXPECT_EQ(cRequestOutOfRangeNrc, ex.GetNrc());
            }
        }

        TEST_F(DownloadServiceTest, OutOfMemoryPoolRequest)
        {
            const std::vector<uint8_t> cOutOfPoolMemoryAddressAndSize{0x08, 0x00, 0x00, 0x40};

            std::future<void> _result{Request(cOutOfPoolMemoryAddressAndSize)};

            try
            {
                _result.get();
                FAIL() << "Expected an NRC exception.";
            }
            catch (const routing::NrcExecption &ex)
            {
                EXPECT_EQ(cUploadDownloadNotAcceptedNrc, ex.GetNrc());
            }
        }

        TEST_F(DownloadServiceTest, PositiveHandleMessage)
        {
            const size_t cSidIndex{0};
            const size_t cMaxNumberOfBlockLengthIndex{2};
            const auto cExpectedSid{
                static_cast<uint8_t>(cSid + cPositiveResponseSidIncrement)};
            const std::vector<uint8_t> cRequestData{
                cSid, cDataFormatIdentifier, cAddressAndLengthFormatIdentifier,
                0x00, 0x10, 0x00, 0x20};

            CancellationHandler _cancellationHandler(false);
            std::future<OperationOutput> _responseFuture{
                Service.HandleMessage(
                    cRequestData, GeneralMetaInfo, std::move(_cancellationHandler))};
            const OperationOutput cResponse{_responseFuture.get()};

            EXPECT_EQ(cExpectedSid, cResponse.responseData.at(cSidIndex));
            EXPECT_EQ(
                cMaxNumberOfBlockLength,
                cResponse.responseData.at(cMaxNumberOfBlockLengthIndex));
        }

        TEST_F(DownloadServiceTest, NegativeHandleMessage)
        {
            const std::vector<uint8_t> cShortRequestData{cSid, cDataFormatIdentifier};
            uint8_t _nrc;

            EXPECT_TRUE(TryGetNrc(&Service, cShortRequestData, _nrc));
            EXPECT_EQ(cIncorrectMessageLengthNrc, _nrc);
        }

        TEST_F(DownloadServiceTest, RepeatedRequest)
        {
            const std::vector<uint8_t> cRequestData{
                cSid, cDataFormatIdentifier, cAddressAndLengthFormatIdentifier,
                0x00, 0x10, 0x00, 0x20};
            uint8_t _nrc;

            CancellationHandler _cancellationHandler(false);
            Service.HandleMessage(
                       cRequestData, GeneralMetaInfo, std::move(_cancellationHandler))
                .get();

            EXPECT_TRUE(TryGetNrc(&Service, cRequestData, _nrc));
            EXPECT_EQ(cUploadDownloadNotAcceptedNrc, _nrc);
        }
    }
}
