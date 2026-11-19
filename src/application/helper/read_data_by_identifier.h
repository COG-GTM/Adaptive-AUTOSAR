#ifndef READ_DATA_BY_IDENTIFIER_H
#define READ_DATA_BY_IDENTIFIER_H

#include <json/json.h>
#include "../../ara/diag/routing/uds_service_router.h"
#include "./curl_wrapper.h"
#include "./memory_cache.h"

namespace application
{
    namespace helper
    {
        /// @brief UDS to read data at the diagnostic server by passing the data ID (DID)
        class ReadDataByIdentifier : public ara::diag::routing::RoutableUdsService
        {
        private:
            static const uint8_t cSid{0x22};
            static const uint16_t cAverageSpeedDid{0xf50d};
            static const uint16_t cFuelAmountDid{0xf52f};
            static const uint16_t cExternalTemperatureDid{0xf546};
            static const uint16_t cAverageFuelConsumptionDid{0xf55e};
            static const uint16_t cEngineCoolantTemperatureDid{0xf505};
            static const uint16_t cOdometerValueDid{0xf5a6};
            static const uint16_t cVinDid{0xf190};
            static const size_t cVinLength{17};
            static const std::chrono::seconds cCacheLifetime;
            static const ara::core::InstanceSpecifier cSpecifer;

            const uint8_t cConditionsNotCorrectNrc{0x22};
            const uint8_t cRequestOutOfRangeNrc{0x31};
            const bool cCollectJsonComments{false};

            const std::string cResourcesUrl;
            const std::string cVin;
            CurlWrapper *mCurl;
            Json::Reader mJsonReader;
            MemoryCache<uint16_t, ara::diag::OperationOutput> mCache;

            static uint16_t getDid(const std::vector<uint8_t> &requestData);
            static void generateResponse(
                uint16_t did,
                ara::diag::OperationOutput &response);

            bool tryGetResourceValue(std::string resourceKey, Json::Value &jsonValue);
            void getAverageSpeed(ara::diag::OperationOutput &response);
            void getFuelAmount(ara::diag::OperationOutput &response);
            void getExternalTemperature(ara::diag::OperationOutput &response);
            void getAverageFuelConsumption(ara::diag::OperationOutput &response);
            void getEngineCoolantTemperature(ara::diag::OperationOutput &response);
            void getOdometerValue(ara::diag::OperationOutput &response);
            void getVin(ara::diag::OperationOutput &response);

        public:
            /// @brief Constructor
            /// @param curl Configured CURL instance for RESTful communication
            /// @param resourcesUrl Connected vehicle resources access RESTful URL
            /// @param vin Vehicle Identification Number served via the DID 0xF190
            /// @note If the VIN is not exactly 17 characters, reading the DID 0xF190 leads to the NRC 0x22.
            ReadDataByIdentifier(
                CurlWrapper *curl,
                std::string resourcesUrl,
                std::string vin);

            std::future<ara::diag::OperationOutput> HandleMessage(
                const std::vector<uint8_t> &requestData,
                ara::diag::MetaInfo &metaInfo,
                ara::diag::CancellationHandler &&cancellationHandler) override;
        };
    }
}

#endif