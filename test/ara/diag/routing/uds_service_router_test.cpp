#include <gtest/gtest.h>
#include "../../../../src/ara/diag/routing/transfer_data.h"
#include "../../../../src/ara/diag/routing/uds_service_router.h"

namespace ara
{
    namespace diag
    {
        namespace routing
        {
            TEST(UdsServiceRouterTest, ValidRouteMethod)
            {
                const uint8_t cExpectedResult{0x21};
                const size_t cSidIndex{1};

                UdsServiceRouter _router;
                std::vector<uint8_t> _requestData{cExpectedResult};
                MetaInfo _metaInfo(Context::kDoIP);
                CancellationHandler _cancellationHandler(false);

                std::future<OperationOutput> _response{
                    _router.Route(
                        _requestData,
                        _metaInfo,
                        std::move(_cancellationHandler))};

                EXPECT_TRUE(_response.valid());
                OperationOutput _operationOutput{_response.get()};
                uint8_t _actualResult{_operationOutput.responseData.at(cSidIndex)};

                EXPECT_EQ(cExpectedResult, _actualResult);
            }

            TEST(UdsServiceRouterTest, InvalidRouteMethod)
            {
                UdsServiceRouter _router;
                std::vector<uint8_t> _requestData;
                MetaInfo _metaInfo(Context::kDoIP);
                CancellationHandler _cancellationHandler(false);

                std::future<OperationOutput> _response{
                    _router.Route(
                        _requestData,
                        _metaInfo,
                        std::move(_cancellationHandler))};

                EXPECT_TRUE(_response.valid());
                EXPECT_THROW(_response.get(), std::invalid_argument);
            }

            TEST(UdsServiceRouterTest, RemoveServiceMethod)
            {
                const uint8_t cSid{0x21};
                UdsServiceRouter _router;

                EXPECT_NO_THROW(_router.RemoveService(cSid));
            }

            TEST(UdsServiceRouterTest, AddedServiceScenario)
            {
                const uint8_t cSid{0x36};
                const std::vector<uint8_t> cNotSupportedResponse{0x7f, cSid, 0x11};
                const std::vector<uint8_t> cServiceResponse{0x7f, cSid, 0x13};

                core::InstanceSpecifier _specifier("Instance0");
                TransferData _service(_specifier);
                UdsServiceRouter _router;
                std::vector<uint8_t> _requestData{cSid};
                MetaInfo _metaInfo(Context::kDoIP);

                _router.AddService(&_service);

                // The added service has not been offered yet.
                CancellationHandler _notOfferedCancellationHandler(false);
                std::future<OperationOutput> _notOfferedFuture{
                    _router.Route(
                        _requestData,
                        _metaInfo,
                        std::move(_notOfferedCancellationHandler))};
                EXPECT_EQ(cNotSupportedResponse, _notOfferedFuture.get().responseData);

                // The request is routed to the offered service which rejects it due to its length.
                _service.Offer();
                CancellationHandler _offeredCancellationHandler(false);
                std::future<OperationOutput> _offeredFuture{
                    _router.Route(
                        _requestData,
                        _metaInfo,
                        std::move(_offeredCancellationHandler))};
                EXPECT_EQ(cServiceResponse, _offeredFuture.get().responseData);

                _router.RemoveService(cSid);
                CancellationHandler _removedCancellationHandler(false);
                std::future<OperationOutput> _removedFuture{
                    _router.Route(
                        _requestData,
                        _metaInfo,
                        std::move(_removedCancellationHandler))};
                EXPECT_EQ(cNotSupportedResponse, _removedFuture.get().responseData);
            }
        }
    }
}