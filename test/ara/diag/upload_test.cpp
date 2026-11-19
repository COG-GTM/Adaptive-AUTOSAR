#include "./routing/testable_uds_service.h"
#include "../../../src/ara/diag/routing/nrc_exception.h"
#include "../../../src/ara/diag/upload.h"

namespace ara
{
    namespace diag
    {
        class UploadServiceTest : public routing::TestableUdsService
        {
        private:
            static core::InstanceSpecifier mSpecifier;

            const std::string cMaxNumberOfBlockLengthKey{"MaxNumberOfBlockLength"};

        protected:
            const uint8_t cMaxNumberOfBlockLength{64};
            const uint8_t cDataFormatIdentifier{0x00};
            const uint8_t cAddressAndLengthFormatIdentifier{0x22};
            const std::vector<uint8_t> cMemoryAddressAndSize{0x02, 0x00, 0x00, 0x04};

            routing::TransferData TransferDataService;
            UploadService Service;

            bool TryGetRequestNrc(
                uint8_t dataFormatIdentifier,
                uint8_t addressAndLengthFormatIdentifier,
                const std::vector<uint8_t> &memoryAddressAndSize,
                uint8_t &nrc)
            {
                CancellationHandler _cancellationHandler(false);
                std::future<void> _future{
                    Service.RequestUpload(
                        dataFormatIdentifier,
                        addressAndLengthFormatIdentifier,
                        memoryAddressAndSize,
                        GeneralMetaInfo,
                        std::move(_cancellationHandler))};

                try
                {
                    _future.get();
                    return false;
                }
                catch (const routing::NrcExecption &_exception)
                {
                    nrc = _exception.GetNrc();
                    return true;
                }
            }

            std::vector<uint8_t> SendTransferData(
                const std::vector<uint8_t> &requestData)
            {
                CancellationHandler _cancellationHandler(false);
                std::future<OperationOutput> _responseFuture{
                    TransferDataService.HandleMessage(
                        requestData, GeneralMetaInfo, std::move(_cancellationHandler))};
                OperationOutput _response{_responseFuture.get()};

                return _response.responseData;
            }

        public:
            UploadServiceTest() : TransferDataService{mSpecifier},
                                Service{mSpecifier, ReentrancyType::kNot, TransferDataService}
            {
                std::string _maxNumberOfBlockLengthStr{
                    std::to_string(cMaxNumberOfBlockLength)};
                GeneralMetaInfo.SetValue(
                    cMaxNumberOfBlockLengthKey, _maxNumberOfBlockLengthStr);
            }
        };

        core::InstanceSpecifier UploadServiceTest::mSpecifier{"Instance0"};

        TEST_F(UploadServiceTest, Constructor)
        {
            const uint8_t cExpectedSid{0x35};

            EXPECT_EQ(cExpectedSid, Service.GetSid());
            EXPECT_FALSE(Service.IsOffered());
        }

        TEST_F(UploadServiceTest, RequestUploadMethod)
        {
            const uint8_t cBlockSequenceCounter{1};
            const size_t cMemorySize{4};
            const size_t cResponseHeaderSize{2};
            const size_t cSidIndex{0};
            const size_t cSequenceCounterIndex{1};
            const uint8_t cExpectedSid{0x76};

            uint8_t _actualNrc;
            EXPECT_FALSE(
                TryGetRequestNrc(
                    cDataFormatIdentifier,
                    cAddressAndLengthFormatIdentifier,
                    cMemoryAddressAndSize,
                    _actualNrc));

            // The transfer data service uploads the requested memory size after the request
            std::vector<uint8_t> _requestData{
                TransferDataService.GetSid(), cBlockSequenceCounter};
            std::vector<uint8_t> _response{SendTransferData(_requestData)};

            ASSERT_EQ(cResponseHeaderSize + cMemorySize, _response.size());
            EXPECT_EQ(cExpectedSid, _response.at(cSidIndex));
            EXPECT_EQ(cBlockSequenceCounter, _response.at(cSequenceCounterIndex));
        }

        TEST_F(UploadServiceTest, UnsupportedDataFormatScenario)
        {
            const uint8_t cExpectedNrc{0x31};
            const uint8_t cCompressedDataFormatIdentifier{0x10};

            uint8_t _actualNrc;
            bool _hasNrc{
                TryGetRequestNrc(
                    cCompressedDataFormatIdentifier,
                    cAddressAndLengthFormatIdentifier,
                    cMemoryAddressAndSize,
                    _actualNrc)};

            EXPECT_TRUE(_hasNrc);
            EXPECT_EQ(cExpectedNrc, _actualNrc);
        }

        TEST_F(UploadServiceTest, UnsupportedLengthFormatScenario)
        {
            const uint8_t cExpectedNrc{0x31};
            const uint8_t cUnsupportedLengthFormatIdentifier{0x44};

            uint8_t _actualNrc;
            bool _hasNrc{
                TryGetRequestNrc(
                    cDataFormatIdentifier,
                    cUnsupportedLengthFormatIdentifier,
                    cMemoryAddressAndSize,
                    _actualNrc)};

            EXPECT_TRUE(_hasNrc);
            EXPECT_EQ(cExpectedNrc, _actualNrc);
        }

        TEST_F(UploadServiceTest, OutOfMemoryPoolScenario)
        {
            const uint8_t cExpectedNrc{0x70};
            const std::vector<uint8_t> cOutOfPoolMemoryAddressAndSize{
                0xff, 0xff, 0x00, 0x04};

            uint8_t _actualNrc;
            bool _hasNrc{
                TryGetRequestNrc(
                    cDataFormatIdentifier,
                    cAddressAndLengthFormatIdentifier,
                    cOutOfPoolMemoryAddressAndSize,
                    _actualNrc)};

            EXPECT_TRUE(_hasNrc);
            EXPECT_EQ(cExpectedNrc, _actualNrc);
        }

        TEST_F(UploadServiceTest, RepeatedRequestScenario)
        {
            const uint8_t cExpectedNrc{0x70};

            uint8_t _actualNrc;
            EXPECT_FALSE(
                TryGetRequestNrc(
                    cDataFormatIdentifier,
                    cAddressAndLengthFormatIdentifier,
                    cMemoryAddressAndSize,
                    _actualNrc));

            // A transfer is not accepted while another one is in progress
            bool _hasNrc{
                TryGetRequestNrc(
                    cDataFormatIdentifier,
                    cAddressAndLengthFormatIdentifier,
                    cMemoryAddressAndSize,
                    _actualNrc)};

            EXPECT_TRUE(_hasNrc);
            EXPECT_EQ(cExpectedNrc, _actualNrc);
        }

        TEST_F(UploadServiceTest, HandleMessageScenario)
        {
            const uint8_t cLengthFormatIdentifier{0x10};
            const std::vector<uint8_t> cExpectedResponse{
                0x75, cLengthFormatIdentifier, cMaxNumberOfBlockLength};

            std::vector<uint8_t> _requestData{
                Service.GetSid(),
                cDataFormatIdentifier,
                cAddressAndLengthFormatIdentifier,
                0x02, 0x00, 0x00, 0x04};
            CancellationHandler _cancellationHandler(false);

            std::future<OperationOutput> _responseFuture{
                Service.HandleMessage(
                    _requestData, GeneralMetaInfo, std::move(_cancellationHandler))};
            OperationOutput _response{_responseFuture.get()};

            EXPECT_EQ(cExpectedResponse, _response.responseData);
        }

        TEST_F(UploadServiceTest, IncorrectMessageLengthScenario)
        {
            const uint8_t cExpectedNrc{0x13};

            uint8_t _actualNrc;
            std::vector<uint8_t> _requestData{
                Service.GetSid(), cDataFormatIdentifier, cAddressAndLengthFormatIdentifier};
            bool _hasNrc{TryGetNrc(&Service, _requestData, _actualNrc)};

            EXPECT_TRUE(_hasNrc);
            EXPECT_EQ(cExpectedNrc, _actualNrc);
        }
    }
}