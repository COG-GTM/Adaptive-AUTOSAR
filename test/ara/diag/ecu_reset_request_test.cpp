#include "./routing/testable_uds_service.h"
#include "../../../src/ara/diag/ecu_reset_request.h"

namespace ara
{
    namespace diag
    {
        class EcuResetRequestTest : public routing::TestableUdsService
        {
        private:
            static core::InstanceSpecifier mSpecifier;

            const std::string cPowerDownTimeKey{"PowerDownTime"};

        protected:
            const uint8_t cPowerDownTime{0xff};

            EcuResetRequest Service;

            bool TryGetResponsePayload(uint8_t subFunction, std::vector<uint8_t> &payload)
            {
                const size_t cSidOffset{0};
                const size_t cSubFunctionOffset{1};
                const size_t cPayloadOffset{2};

                auto _expectedSid{static_cast<uint8_t>(Service.GetSid() + 0x40)};

                std::vector<uint8_t> _requestData{Service.GetSid(), subFunction};
                CancellationHandler _cancellationHandler(false);

                std::future<OperationOutput> _responseFuture{
                    Service.HandleMessage(
                        _requestData,
                        GeneralMetaInfo,
                        std::move(_cancellationHandler))};

                OperationOutput _response{_responseFuture.get()};

                uint8_t _actualSid{_response.responseData.at(cSidOffset)};
                uint8_t _actualSubFunction{_response.responseData.at(cSubFunctionOffset)};

                bool _result{
                    (_expectedSid == _actualSid) && (subFunction == _actualSubFunction)};

                if (_result)
                {
                    payload =
                    std::vector<uint8_t>(
                        _response.responseData.begin() + cPayloadOffset,
                        _response.responseData.end());
                }

                return _result;
            }

        public:
            EcuResetRequestTest() : Service{mSpecifier}
            {
                std::string _powerDownTimeStr{std::to_string(cPowerDownTime)};
                GeneralMetaInfo.SetValue(cPowerDownTimeKey, _powerDownTimeStr);
            }
        };

        core::InstanceSpecifier EcuResetRequestTest::mSpecifier{"Instance0"};

        TEST_F(EcuResetRequestTest, IncorrectRequestScenario)
        {
            const uint8_t cExpectedNrc{0x13};

            uint8_t _actualNrc;
            std::vector<uint8_t> _requestData{Service.GetSid()};
            bool _hasNrc{TryGetNrc(&Service, _requestData, _actualNrc)};

            EXPECT_TRUE(_hasNrc);
            EXPECT_EQ(cExpectedNrc, _actualNrc);
        }

        TEST_F(EcuResetRequestTest, InvalidSubFunctionScenario)
        {
            const uint8_t cExpectedNrc{0x12};
            const uint8_t cSubFunction{0x06};

            uint8_t _actualNrc;
            std::vector<uint8_t> _requestData{Service.GetSid(), cSubFunction};
            bool _hasNrc{TryGetNrc(&Service, _requestData, _actualNrc)};

            EXPECT_TRUE(_hasNrc);
            EXPECT_EQ(cExpectedNrc, _actualNrc);
        }

        TEST_F(EcuResetRequestTest, RequestResetMehod)
        {
            const ResetRequestType cResetRequest{ResetRequestType::kHardReset};

            core::Optional<uint8_t> _id{0x00};

            CancellationHandler _validCancellationHander(false);
            std::future<void> _validFuture{
                Service.RequestReset(
                    cResetRequest,
                    _id,
                    GeneralMetaInfo,
                    std::move(_validCancellationHander))};
            EXPECT_TRUE(_validFuture.valid());

            CancellationHandler _invalidCancellationHander(false);
            std::future<void> _invalidFuture{
                Service.RequestReset(
                    cResetRequest,
                    _id,
                    GeneralMetaInfo,
                    std::move(_invalidCancellationHander))};
            EXPECT_FALSE(_invalidFuture.valid());
        }

        TEST_F(EcuResetRequestTest, ExecuteResetMethod)
        {
            const ResetRequestType cResetRequest{ResetRequestType::kHardReset};

            core::Optional<uint8_t> _id;
            CancellationHandler _cancellationHander(false);

            EXPECT_THROW(Service.ExecuteReset(GeneralMetaInfo), std::logic_error);

            std::future<void> _succeedFuture{
                Service.RequestReset(
                    cResetRequest,
                    _id,
                    GeneralMetaInfo,
                    std::move(_cancellationHander))};
            EXPECT_TRUE(_succeedFuture.valid());
        }

        TEST_F(EcuResetRequestTest, EnableRapidShutdownMethod)
        {
            const bool cEnable{false};

            CancellationHandler _cancellationHander(false);

            std::future<void> _succeedFuture{
                Service.EnableRapidShutdown(
                    cEnable,
                    GeneralMetaInfo,
                    std::move(_cancellationHander))};

            EXPECT_TRUE(_succeedFuture.valid());
        }

        TEST_F(EcuResetRequestTest, RequestResetScenario)
        {
            const uint8_t cSubFunction{0x01};
            const size_t cExpectedPayloadSize{0};

            std::vector<uint8_t> _payload;

            EXPECT_TRUE(TryGetResponsePayload(cSubFunction, _payload));

            size_t _actualPayloadSize{_payload.size()};
            EXPECT_EQ(cExpectedPayloadSize, _actualPayloadSize);
        }

        TEST_F(EcuResetRequestTest, EnableRapidShutdownScenario)
        {
            const uint8_t cSubFunction{0x04};
            const size_t cPowerDownTimeOffset{0};

            std::vector<uint8_t> _payload;

            EXPECT_TRUE(TryGetResponsePayload(cSubFunction, _payload));

            uint8_t _actualPowerDownTime{_payload.at(cPowerDownTimeOffset)};
            EXPECT_EQ(cPowerDownTime, _actualPowerDownTime);
        }

        TEST_F(EcuResetRequestTest, KeyOffOnResetScenario)
        {
            const uint8_t cSubFunction{0x02};

            std::vector<uint8_t> _payload;

            EXPECT_TRUE(TryGetResponsePayload(cSubFunction, _payload));
            EXPECT_TRUE(_payload.empty());
        }

        TEST_F(EcuResetRequestTest, SoftResetScenario)
        {
            const uint8_t cSubFunction{0x03};

            std::vector<uint8_t> _payload;

            EXPECT_TRUE(TryGetResponsePayload(cSubFunction, _payload));
            EXPECT_TRUE(_payload.empty());
        }

        TEST_F(EcuResetRequestTest, DisableRapidShutdownScenario)
        {
            const uint8_t cSubFunction{0x05};

            std::vector<uint8_t> _payload;

            EXPECT_TRUE(TryGetResponsePayload(cSubFunction, _payload));
            // No power down time is reported while disabling the rapid shutdown
            EXPECT_TRUE(_payload.empty());
        }

        TEST_F(EcuResetRequestTest, RepeatedResetRequestScenario)
        {
            const uint8_t cExpectedNrc{0x12};
            const uint8_t cHardResetSubFunction{0x01};
            const uint8_t cSoftResetSubFunction{0x03};

            std::vector<uint8_t> _payload;
            EXPECT_TRUE(TryGetResponsePayload(cHardResetSubFunction, _payload));

            // A reset type can be requested only once
            uint8_t _actualNrc;
            std::vector<uint8_t> _requestData{Service.GetSid(), cSoftResetSubFunction};
            bool _hasNrc{TryGetNrc(&Service, _requestData, _actualNrc)};

            EXPECT_TRUE(_hasNrc);
            EXPECT_EQ(cExpectedNrc, _actualNrc);
        }

        TEST_F(EcuResetRequestTest, EnableRapidShutdownWithoutPowerDownTime)
        {
            const std::vector<uint8_t> cExpectedResponse{0x7f, 0x11, 0x12};
            const uint8_t cSubFunction{0x04};

            MetaInfo _metaInfo(Context::kDoIP);
            std::vector<uint8_t> _requestData{Service.GetSid(), cSubFunction};
            CancellationHandler _cancellationHandler(false);

            std::future<OperationOutput> _responseFuture{
                Service.HandleMessage(
                    _requestData, _metaInfo, std::move(_cancellationHandler))};
            OperationOutput _response{_responseFuture.get()};

            EXPECT_EQ(cExpectedResponse, _response.responseData);
        }

        TEST_F(EcuResetRequestTest, ExecuteRequestedReset)
        {
            core::Optional<uint8_t> _id;
            CancellationHandler _cancellationHander(false);

            std::future<void> _succeedFuture{
                Service.RequestReset(
                    ResetRequestType::kSoftReset,
                    _id,
                    GeneralMetaInfo,
                    std::move(_cancellationHander))};
            EXPECT_TRUE(_succeedFuture.valid());

            // Executing the reset is not supported even after requesting a reset type.
            EXPECT_THROW(Service.ExecuteReset(GeneralMetaInfo), std::logic_error);
        }
    }
}