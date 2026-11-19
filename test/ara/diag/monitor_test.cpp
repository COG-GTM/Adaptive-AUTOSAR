#include <gtest/gtest.h>
#include "../../../src/ara/diag/monitor.h"

namespace ara
{
    namespace diag
    {
        TEST(MonitorTest, CounterBasedOfferScenario)
        {
            core::InstanceSpecifier _specifier("Instance0");
            InitMonitorReason _currentReason{InitMonitorReason::kClear};
            auto _initMonitor = [&](InitMonitorReason newReason)
            {
                _currentReason = newReason;
            };
            CounterBased _defaultValues;

            Monitor _monitor(_specifier, _initMonitor, _defaultValues);

            core::Result<void> _result{_monitor.Offer()};
            EXPECT_TRUE(_result.HasValue());
            EXPECT_EQ(InitMonitorReason::kReenabled, _currentReason);

            core::Result<void> _newResult{_monitor.Offer()};
            EXPECT_FALSE(_newResult.HasValue());

            _monitor.StopOffer();
            EXPECT_EQ(InitMonitorReason::kDisabled, _currentReason);
        }

        TEST(MonitorTest, TimerBasedOfferScenario)
        {
            core::InstanceSpecifier _specifier("Instance0");
            InitMonitorReason _currentReason{InitMonitorReason::kClear};
            auto _initMonitor = [&](InitMonitorReason newReason)
            {
                _currentReason = newReason;
            };
            TimeBased _defaultValues;

            Monitor _monitor(_specifier, _initMonitor, _defaultValues);

            core::Result<void> _result{_monitor.Offer()};
            EXPECT_TRUE(_result.HasValue());
            EXPECT_EQ(InitMonitorReason::kReenabled, _currentReason);

            core::Result<void> _newResult{_monitor.Offer()};
            EXPECT_FALSE(_newResult.HasValue());

            _monitor.StopOffer();
            EXPECT_EQ(InitMonitorReason::kDisabled, _currentReason);
        }

        static CounterBased GetCounterBasedDefaults()
        {
            CounterBased _result;
            _result.failedThreshold = 10;
            _result.passedThreshold = -10;
            _result.failedStepsize = 5;
            _result.passedStepsize = 5;
            _result.failedJumpValue = 0;
            _result.passedJumpValue = 0;
            _result.useJumpToFailed = false;
            _result.useJumpToPassed = false;

            return _result;
        }

        TEST(MonitorTest, CounterBasedFailedAndPassedActions)
        {
            const int8_t cFailedFdc{127};
            const int8_t cPassedFdc{-128};
            const uint8_t cFailedStatus{0x80};
            const uint8_t cPassedStatus{0x00};

            core::InstanceSpecifier _specifier("Instance0");
            Event _event(_specifier);
            Monitor _monitor(_specifier, nullptr, GetCounterBasedDefaults());
            _monitor.AttachEvent(&_event);
            _monitor.Offer();

            _monitor.ReportMonitorAction(MonitorAction::kFailed);
            EXPECT_EQ(cFailedFdc, _event.GetFaultDetectionCounter().Value());
            EXPECT_EQ(cFailedStatus, _event.GetEventStatus().Value().encodedBits);

            _monitor.ReportMonitorAction(MonitorAction::kPassed);
            EXPECT_EQ(cPassedFdc, _event.GetFaultDetectionCounter().Value());
            EXPECT_EQ(cPassedStatus, _event.GetEventStatus().Value().encodedBits);
        }

        TEST(MonitorTest, CounterBasedPrefailedAction)
        {
            const int8_t cNeutralFdc{0};
            const int8_t cFailedFdc{127};

            core::InstanceSpecifier _specifier("Instance0");
            Event _event(_specifier);
            Monitor _monitor(_specifier, nullptr, GetCounterBasedDefaults());
            _monitor.AttachEvent(&_event);
            _monitor.Offer();

            // First pre-failed report is below the failed threshold
            _monitor.ReportMonitorAction(MonitorAction::kPrefailed);
            EXPECT_EQ(cNeutralFdc, _event.GetFaultDetectionCounter().Value());

            // Second pre-failed report reaches the failed threshold
            _monitor.ReportMonitorAction(MonitorAction::kPrefailed);
            EXPECT_EQ(cFailedFdc, _event.GetFaultDetectionCounter().Value());
        }

        TEST(MonitorTest, CounterBasedPrepassedAction)
        {
            const int8_t cNeutralFdc{0};
            const int8_t cPassedFdc{-128};

            core::InstanceSpecifier _specifier("Instance0");
            Event _event(_specifier);
            Monitor _monitor(_specifier, nullptr, GetCounterBasedDefaults());
            _monitor.AttachEvent(&_event);
            _monitor.Offer();

            _monitor.ReportMonitorAction(MonitorAction::kPrepassed);
            EXPECT_EQ(cNeutralFdc, _event.GetFaultDetectionCounter().Value());

            _monitor.ReportMonitorAction(MonitorAction::kPrepassed);
            EXPECT_EQ(cPassedFdc, _event.GetFaultDetectionCounter().Value());
        }

        TEST(MonitorTest, ResetDebouncingAction)
        {
            const int8_t cNeutralFdc{0};
            const int8_t cFailedFdc{127};

            core::InstanceSpecifier _specifier("Instance0");
            Event _event(_specifier);
            Monitor _monitor(_specifier, nullptr, GetCounterBasedDefaults());
            _monitor.AttachEvent(&_event);
            _monitor.Offer();

            _monitor.ReportMonitorAction(MonitorAction::kPrefailed);
            _monitor.ReportMonitorAction(MonitorAction::kResetDebouncing);

            // Due to the reset, a single pre-failed report cannot reach the threshold
            _monitor.ReportMonitorAction(MonitorAction::kPrefailed);
            EXPECT_EQ(cNeutralFdc, _event.GetFaultDetectionCounter().Value());

            _monitor.ReportMonitorAction(MonitorAction::kPrefailed);
            EXPECT_EQ(cFailedFdc, _event.GetFaultDetectionCounter().Value());
        }

        TEST(MonitorTest, FreezeDebouncingAction)
        {
            const int8_t cNeutralFdc{0};

            core::InstanceSpecifier _specifier("Instance0");
            Event _event(_specifier);
            Monitor _monitor(_specifier, nullptr, GetCounterBasedDefaults());
            _monitor.AttachEvent(&_event);
            _monitor.Offer();

            _monitor.ReportMonitorAction(MonitorAction::kPrefailed);
            // Freezing a counter-based debouncer resets its counter
            _monitor.ReportMonitorAction(MonitorAction::kFreezeDebouncing);
            _monitor.ReportMonitorAction(MonitorAction::kPrefailed);

            EXPECT_EQ(cNeutralFdc, _event.GetFaultDetectionCounter().Value());
        }

        TEST(MonitorTest, ResetTestFailedAction)
        {
            const uint8_t cFailedStatus{0x80};
            const uint8_t cResetStatus{0x00};

            core::InstanceSpecifier _specifier("Instance0");
            Event _event(_specifier);
            Monitor _monitor(_specifier, nullptr, GetCounterBasedDefaults());
            _monitor.AttachEvent(&_event);
            _monitor.Offer();

            _monitor.ReportMonitorAction(MonitorAction::kFailed);
            EXPECT_EQ(cFailedStatus, _event.GetEventStatus().Value().encodedBits);

            _monitor.ReportMonitorAction(MonitorAction::kResetTestFailed);
            EXPECT_EQ(cResetStatus, _event.GetEventStatus().Value().encodedBits);
        }

        TEST(MonitorTest, ActionsWithoutAttachedEvent)
        {
            core::InstanceSpecifier _specifier("Instance0");
            Monitor _monitor(_specifier, nullptr, GetCounterBasedDefaults());
            _monitor.Offer();

            EXPECT_NO_THROW(_monitor.ReportMonitorAction(MonitorAction::kFailed));
            EXPECT_NO_THROW(_monitor.ReportMonitorAction(MonitorAction::kPassed));
            EXPECT_NO_THROW(_monitor.ReportMonitorAction(MonitorAction::kResetTestFailed));
        }

        TEST(MonitorTest, UnsupportedAction)
        {
            core::InstanceSpecifier _specifier("Instance0");
            Monitor _monitor(_specifier, nullptr, GetCounterBasedDefaults());
            _monitor.Offer();

            EXPECT_THROW(
                _monitor.ReportMonitorAction(MonitorAction::kFdcThresholdReached),
                std::invalid_argument);
        }

        TEST(MonitorTest, ActionsIgnoredWhileNotOffered)
        {
            const int8_t cNeutralFdc{0};
            const uint8_t cInitialStatus{0x02};

            core::InstanceSpecifier _specifier("Instance0");
            Event _event(_specifier);
            Monitor _monitor(_specifier, nullptr, GetCounterBasedDefaults());
            _monitor.AttachEvent(&_event);

            _monitor.ReportMonitorAction(MonitorAction::kFailed);
            EXPECT_EQ(cNeutralFdc, _event.GetFaultDetectionCounter().Value());
            EXPECT_EQ(cInitialStatus, _event.GetEventStatus().Value().encodedBits);

            EXPECT_NO_THROW(
                _monitor.ReportMonitorAction(MonitorAction::kFdcThresholdReached));

            _monitor.Offer();
            _monitor.StopOffer();
            _monitor.ReportMonitorAction(MonitorAction::kFailed);
            EXPECT_EQ(cNeutralFdc, _event.GetFaultDetectionCounter().Value());
        }

        TEST(MonitorTest, StopOfferWhileNotOffered)
        {
            const uint32_t cExpectedCallCount{0};

            core::InstanceSpecifier _specifier("Instance0");
            uint32_t _callCount{0};
            std::function<void(InitMonitorReason)> _initMonitor{
                [&_callCount](InitMonitorReason)
                {
                    ++_callCount;
                }};

            Monitor _monitor(_specifier, _initMonitor, GetCounterBasedDefaults());
            _monitor.StopOffer();

            EXPECT_EQ(cExpectedCallCount, _callCount);
        }

        TEST(MonitorTest, TimerBasedFailedAndPassedActions)
        {
            const int8_t cFailedFdc{127};
            const int8_t cPassedFdc{-128};

            core::InstanceSpecifier _specifier("Instance0");
            Event _event(_specifier);
            TimeBased _defaultValues;
            _defaultValues.passedMs = 10;
            _defaultValues.failedMs = 10;

            Monitor _monitor(_specifier, nullptr, _defaultValues);
            _monitor.AttachEvent(&_event);
            _monitor.Offer();

            _monitor.ReportMonitorAction(MonitorAction::kFailed);
            EXPECT_EQ(cFailedFdc, _event.GetFaultDetectionCounter().Value());

            _monitor.ReportMonitorAction(MonitorAction::kPassed);
            EXPECT_EQ(cPassedFdc, _event.GetFaultDetectionCounter().Value());
        }
    }
}