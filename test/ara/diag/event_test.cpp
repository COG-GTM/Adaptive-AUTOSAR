#include <gtest/gtest.h>
#include "../../../src/ara/diag/event.h"

namespace ara
{
    namespace diag
    {
        class EventTest : public testing::Test
        {
        protected:
            const uint8_t cInitialEventState{0x02};

            ara::core::InstanceSpecifier Specifier;
            uint8_t NotifiedEventStatus;

        public:
            EventTest() : Specifier{"Instance0"},
                          NotifiedEventStatus{cInitialEventState}
            {
            }

            void OnEventStatusChanged(EventStatusByte eventStatus)
            {
                NotifiedEventStatus = eventStatus.encodedBits;
            }
        };

        TEST_F(EventTest, Constructor)
        {
            Event _event(Specifier);
            auto _eventStatus{_event.GetEventStatus()};
            uint8_t _actualResult{_eventStatus.Value().encodedBits};

            EXPECT_EQ(cInitialEventState, _actualResult);
        };

        TEST_F(EventTest, EventStatusProperty)
        {
            const uint8_t cExpectedResult{0x82};

            Event _event(Specifier);
            auto _notifier{
                std::bind(
                    &EventTest::OnEventStatusChanged, this, std::placeholders::_1)};

            _event.SetEventStatusChangedNotifier(_notifier);
            _event.SetEventStatusBits({{EventStatusBit::kTestFailed, true}});
            auto _eventStatus{_event.GetEventStatus()};

            EXPECT_EQ(cExpectedResult, NotifiedEventStatus);
        };

        TEST_F(EventTest, LatchedWIRStatusProperty)
        {
            const bool cExpectedResult{true};

            Event _event(Specifier);
            auto _succeed{_event.SetLatchedWIRStatus(cExpectedResult)};
            EXPECT_TRUE(_succeed.HasValue());
            auto _actualResult{_event.GetLatchedWIRStatus()};

            EXPECT_EQ(cExpectedResult, _actualResult.Value());
        }

        TEST_F(EventTest, DtcNumberProperty)
        {
            const uint32_t cExpectedResult{1};

            Event _event(Specifier);
            _event.SetDTCNumber(cExpectedResult);
            auto _actualResult{_event.GetDTCNumber(DTCFormatType::kDTCFormatUDS)};

            EXPECT_EQ(cExpectedResult, _actualResult.Value());
        }

        TEST_F(EventTest, FdcProperty)
        {
            const uint32_t cExpectedResult{1};

            Event _event(Specifier);
            _event.SetFaultDetectionCounter(cExpectedResult);
            auto _actualResult{_event.GetFaultDetectionCounter()};

            EXPECT_EQ(cExpectedResult, _actualResult.Value());
        }

        TEST_F(EventTest, DebouncingStatusProperty)
        {
            const int8_t cFdcMin{-128};
            const int8_t cFdcNeg{-1};
            const int8_t cFdcZero{0};
            const int8_t cFdcPos{1};
            const int8_t cFdcMax{127};

            Event _event(Specifier);

            _event.SetFaultDetectionCounter(cFdcMin);
            EXPECT_EQ(DebouncingState::kFinallyHealed, _event.GetDebouncingStatus());

            _event.SetFaultDetectionCounter(cFdcNeg);
            EXPECT_EQ(DebouncingState::kTemporarilyHealed, _event.GetDebouncingStatus());

            _event.SetFaultDetectionCounter(cFdcZero);
            EXPECT_EQ(DebouncingState::kNeutral, _event.GetDebouncingStatus());

            _event.SetFaultDetectionCounter(cFdcPos);
            EXPECT_EQ(DebouncingState::kTemporarilyDefective, _event.GetDebouncingStatus());

            _event.SetFaultDetectionCounter(cFdcMax);
            EXPECT_EQ(DebouncingState::kFinallyDefective, _event.GetDebouncingStatus());
        }

        TEST_F(EventTest, TestCompleteProperty)
        {
            const int8_t cFdcMin{-128};
            const int8_t cFdcZero{0};
            const int8_t cFdcMax{127};

            Event _event(Specifier);

            _event.SetFaultDetectionCounter(cFdcMin);
            EXPECT_TRUE(_event.GetTestComplete().Value());

            _event.SetFaultDetectionCounter(cFdcZero);
            EXPECT_FALSE(_event.GetTestComplete().Value());

            _event.SetFaultDetectionCounter(cFdcMax);
            EXPECT_TRUE(_event.GetTestComplete().Value());
        }

        TEST_F(EventTest, EventStatusBitsEncoding)
        {
            const uint8_t cFailedThisCycleStatus{0x42};
            const uint8_t cCompletedThisCycleStatus{0x40};

            Event _event(Specifier);

            _event.SetEventStatusBits(
                {{EventStatusBit::kTestFailedThisOperationCycle, true}});
            EXPECT_EQ(
                cFailedThisCycleStatus, _event.GetEventStatus().Value().encodedBits);

            _event.SetEventStatusBits(
                {{EventStatusBit::kTestNotCompletedThisOperationCycle, false}});
            EXPECT_EQ(
                cCompletedThisCycleStatus, _event.GetEventStatus().Value().encodedBits);
        }

        TEST_F(EventTest, UnchangedEventStatusIsNotNotified)
        {
            const uint32_t cExpectedCallCount{0};

            uint32_t _callCount{0};
            std::function<void(EventStatusByte)> _notifier{
                [&_callCount](EventStatusByte)
                {
                    ++_callCount;
                }};

            Event _event(Specifier);
            _event.SetEventStatusChangedNotifier(_notifier);

            std::map<EventStatusBit, bool> _emptyStatusBits;
            _event.SetEventStatusBits(_emptyStatusBits);
            // The bit is already set within the default event status
            _event.SetEventStatusBits(
                {{EventStatusBit::kTestNotCompletedThisOperationCycle, true}});

            EXPECT_EQ(cExpectedCallCount, _callCount);
            EXPECT_EQ(cInitialEventState, _event.GetEventStatus().Value().encodedBits);
        }

        TEST_F(EventTest, InvalidEventStatusBit)
        {
            const EventStatusBit cInvalidBit{static_cast<EventStatusBit>(0x7f)};

            Event _event(Specifier);

            EXPECT_THROW(
                _event.SetEventStatusBits({{cInvalidBit, true}}), std::out_of_range);
            EXPECT_EQ(cInitialEventState, _event.GetEventStatus().Value().encodedBits);
        }

        TEST_F(EventTest, UnsupportedDtcFormat)
        {
            Event _event(Specifier);

            EXPECT_THROW(
                _event.GetDTCNumber(DTCFormatType::kDTCFormatOBD),
                std::invalid_argument);
            EXPECT_THROW(
                _event.GetDTCNumber(DTCFormatType::kDTCFormatJ1939),
                std::invalid_argument);
        }
    }
}