#include "./routing/testable_uds_service.h"
#include "../../../src/ara/diag/generic_routine.h"

namespace ara
{
    namespace diag
    {
        static const ara::core::InstanceSpecifier cSpecifier("Instance0");

        class GenericRoutineTest : public GenericRoutine, public routing::TestableUdsService
        {
        private:
            static const ReentrancyType cReentrancy{ReentrancyType::kNot};

            std::future<OperationOutput> GetDummyResponse()
            {
                OperationOutput _dummyResponse;
                std::promise<OperationOutput> _resultPromise;
                std::future<OperationOutput> _result{_resultPromise.get_future()};
                _resultPromise.set_value(_dummyResponse);

                return _result;
            }

        public:
            GenericRoutineTest() : GenericRoutine(cSpecifier, cReentrancy)
            {
            }

            std::future<OperationOutput> Start(
                uint16_t routineId,
                std::vector<uint8_t> requestData,
                MetaInfo &metaInfo,
                CancellationHandler &&cancellationHandler) override
            {
                return GetDummyResponse();
            }

            std::future<OperationOutput> Stop(
                uint16_t routineId,
                std::vector<uint8_t> requestData,
                MetaInfo &metaInfo,
                CancellationHandler &&cancellationHandler) override
            {
                return GetDummyResponse();
            }

            std::future<OperationOutput> RequestResults(
                uint16_t routineId,
                std::vector<uint8_t> requestData,
                MetaInfo &metaInfo,
                CancellationHandler &&cancellationHandler) override
            {
                return GetDummyResponse();
            }
        };

        const ReentrancyType GenericRoutineTest::cReentrancy;

        TEST_F(GenericRoutineTest, IncorrectRequestScenario)
        {
            const uint8_t cExpectedNrc{0x13};

            uint8_t _actualNrc;
            std::vector<uint8_t> _requestData{GetSid()};
            bool _hasNrc{TryGetNrc(this, _requestData, _actualNrc)};

            EXPECT_TRUE(_hasNrc);
            EXPECT_EQ(cExpectedNrc, _actualNrc);
        }

        TEST_F(GenericRoutineTest, InvalidSubFunctionScenario)
        {
            const uint8_t cExpectedNrc{0x12};
            const uint8_t cSubFunction{0xff};
            const uint8_t cRoutineIdMsb{0x02};
            const uint8_t cRoutineIdLsb{0x01};

            uint8_t _actualNrc;
            std::vector<uint8_t> _requestData{
                GetSid(), cSubFunction, cRoutineIdMsb, cRoutineIdLsb};
            bool _hasNrc{TryGetNrc(this, _requestData, _actualNrc)};

            EXPECT_TRUE(_hasNrc);
            EXPECT_EQ(cExpectedNrc, _actualNrc);
        }

        enum class RoutineCall : uint8_t
        {
            kNone = 0x00,
            kStart = 0x01,
            kStop = 0x02,
            kRequestResults = 0x03
        };

        class GenericRoutineDispatchTest : public GenericRoutine, public routing::TestableUdsService
        {
        private:
            std::future<OperationOutput> Record(RoutineCall call, uint16_t routineId)
            {
                LastCall = call;
                LastRoutineId = routineId;

                OperationOutput _response;
                std::promise<OperationOutput> _resultPromise;
                std::future<OperationOutput> _result{_resultPromise.get_future()};
                _resultPromise.set_value(_response);

                return _result;
            }

        protected:
            const uint16_t cRoutineId{0x0201};

            RoutineCall LastCall;
            uint16_t LastRoutineId;

            void Dispatch(uint8_t subFunction)
            {
                const uint8_t cRoutineIdMsb{0x02};
                const uint8_t cRoutineIdLsb{0x01};

                std::vector<uint8_t> _requestData{
                    GetSid(), subFunction, cRoutineIdMsb, cRoutineIdLsb};
                CancellationHandler _cancellationHandler(false);

                std::future<OperationOutput> _responseFuture{
                    HandleMessage(
                        _requestData,
                        GeneralMetaInfo,
                        std::move(_cancellationHandler))};
                _responseFuture.get();
            }

        public:
            GenericRoutineDispatchTest() : GenericRoutine(cSpecifier, ReentrancyType::kNot),
                                           LastCall{RoutineCall::kNone},
                                           LastRoutineId{0}
            {
            }

            std::future<OperationOutput> Start(
                uint16_t routineId,
                std::vector<uint8_t> requestData,
                MetaInfo &metaInfo,
                CancellationHandler &&cancellationHandler) override
            {
                return Record(RoutineCall::kStart, routineId);
            }

            std::future<OperationOutput> Stop(
                uint16_t routineId,
                std::vector<uint8_t> requestData,
                MetaInfo &metaInfo,
                CancellationHandler &&cancellationHandler) override
            {
                return Record(RoutineCall::kStop, routineId);
            }

            std::future<OperationOutput> RequestResults(
                uint16_t routineId,
                std::vector<uint8_t> requestData,
                MetaInfo &metaInfo,
                CancellationHandler &&cancellationHandler) override
            {
                return Record(RoutineCall::kRequestResults, routineId);
            }
        };

        TEST_F(GenericRoutineDispatchTest, StartScenario)
        {
            const uint8_t cSubFunction{0x01};

            Dispatch(cSubFunction);

            EXPECT_EQ(RoutineCall::kStart, LastCall);
            EXPECT_EQ(cRoutineId, LastRoutineId);
        }

        TEST_F(GenericRoutineDispatchTest, StopScenario)
        {
            const uint8_t cSubFunction{0x02};

            Dispatch(cSubFunction);

            EXPECT_EQ(RoutineCall::kStop, LastCall);
            EXPECT_EQ(cRoutineId, LastRoutineId);
        }

        TEST_F(GenericRoutineDispatchTest, RequestResultsScenario)
        {
            const uint8_t cSubFunction{0x03};

            Dispatch(cSubFunction);

            EXPECT_EQ(RoutineCall::kRequestResults, LastCall);
            EXPECT_EQ(cRoutineId, LastRoutineId);
        }

        TEST_F(GenericRoutineDispatchTest, UnsupportedSubFunctionIsNotDispatched)
        {
            const uint8_t cSubFunction{0x04};
            const uint8_t cExpectedNrc{0x12};

            uint8_t _actualNrc;
            std::vector<uint8_t> _requestData{GetSid(), cSubFunction, 0x02, 0x01};
            bool _hasNrc{TryGetNrc(this, _requestData, _actualNrc)};

            EXPECT_TRUE(_hasNrc);
            EXPECT_EQ(cExpectedNrc, _actualNrc);
            EXPECT_EQ(RoutineCall::kNone, LastCall);
        }
    }
}