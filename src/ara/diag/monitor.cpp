#include "./monitor.h"
#include "./diag_error_domain.h"

namespace ara
{
    namespace diag
    {
        Monitor::Monitor(
            const core::InstanceSpecifier &specifier,
            std::function<void(InitMonitorReason)> initMonitor) : mSpecifier{specifier},
                                                                  mInitMonitor{initMonitor},
                                                                  mOffered{false},
                                                                  mEvent{nullptr},
                                                                  mLogger{ara::log::Logger::CreateLogger("DMON", "Diagnostic Monitor", ara::log::LogLevel::kDebug)}
        {
        }

        void Monitor::onEventStatusChanged(bool passed)
        {
            const int8_t cFailedFdc{127};
            const int8_t cPassedFdc{-128};
            const bool cTestNotCompleted{false};

            if (mEvent)
            {
                mLogger.LogDebug() << "Event status changed, passed: " << passed;
                mEvent->SetFaultDetectionCounter(passed ? cPassedFdc : cFailedFdc);
                mEvent->SetEventStatusBits(
                    {{EventStatusBit::kTestFailed, !passed},
                     {EventStatusBit::kTestNotCompletedThisOperationCycle, cTestNotCompleted}});
            }
            else
            {
                mLogger.LogWarn() << "Event status changed but no event attached";
            }
        }

        Monitor::Monitor(
            const core::InstanceSpecifier &specifier,
            std::function<void(InitMonitorReason)> initMonitor,
            CounterBased defaultValues) : Monitor(specifier, initMonitor)
        {
            auto _callback{
                std::bind(
                    &Monitor::onEventStatusChanged, this, std::placeholders::_1)};

            mDebouncer =
                new debouncing::CounterBasedDebouncer(_callback, defaultValues);

            mLogger.LogInfo() << "Monitor created with counter-based debouncing for specifier: " << mSpecifier;
        }

        Monitor::Monitor(
            const core::InstanceSpecifier &specifier,
            std::function<void(InitMonitorReason)> initMonitor,
            TimeBased defaultValues) : Monitor(specifier, initMonitor)
        {
            auto _callback{
                std::bind(
                    &Monitor::onEventStatusChanged, this, std::placeholders::_1)};

            mDebouncer =
                new debouncing::TimerBasedDebouncer(_callback, defaultValues);

            mLogger.LogInfo() << "Monitor created with timer-based debouncing for specifier: " << mSpecifier;
        }

        void Monitor::ReportMonitorAction(MonitorAction action)
        {
            if (mOffered)
            {
                mLogger.LogDebug() << "Monitor action reported: " << static_cast<uint32_t>(action);

                switch (action)
                {
                case MonitorAction::kPassed:
                    mDebouncer->ReportPassed();
                    break;

                case MonitorAction::kFailed:
                    mDebouncer->ReportFailed();
                    break;

                case MonitorAction::kPrepassed:
                    mDebouncer->ReportPrepassed();
                    break;

                case MonitorAction::kPrefailed:
                    mDebouncer->ReportPrefailed();
                    break;

                case MonitorAction::kFreezeDebouncing:
                    mDebouncer->Freeze();
                    break;

                case MonitorAction::kResetDebouncing:
                    mDebouncer->Reset();
                    break;

                case MonitorAction::kResetTestFailed:
                    if (mEvent)
                    {
                        mEvent->SetEventStatusBits({{EventStatusBit::kTestFailed, false}});
                    }
                    break;

                default:
                    mLogger.LogError() << "Unsupported monitor action reported";
                    throw std::invalid_argument("Reported monitor action is not supported.");
                }
            }
            else
            {
                mLogger.LogWarn() << "ReportMonitorAction called but monitor is not offered";
            }
        }

        void Monitor::AttachEvent(Event *event)
        {
            mEvent = event;
            mLogger.LogInfo() << "Event attached to monitor";
        }

        core::Result<void> Monitor::Offer()
        {
            if (mOffered)
            {
                mLogger.LogWarn() << "Monitor already offered";

                core::ErrorDomain *_errorDomain{DiagErrorDomain::GetDiagDomain()};
                auto _diagErrorDomain{static_cast<DiagErrorDomain *>(_errorDomain)};
                core::ErrorCode _errorCode{_diagErrorDomain->MakeErrorCode(DiagErrc::kAlreadyOffered)};
                auto _result{core::Result<void>::FromError(_errorCode)};

                return _result;
            }
            else
            {
                mOffered = true;
                core::Result<void> _result;
                if (mInitMonitor)
                {
                    mInitMonitor(InitMonitorReason::kReenabled);
                }

                mLogger.LogInfo() << "Monitor offered successfully";

                return _result;
            }
        }

        void Monitor::StopOffer()
        {
            if (mOffered)
            {
                mOffered = false;

                if (mInitMonitor)
                {
                    mInitMonitor(InitMonitorReason::kDisabled);
                }

                mLogger.LogInfo() << "Monitor stopped offering";
            }
            else
            {
                mLogger.LogDebug() << "StopOffer called but monitor was not offering";
            }
        }

        Monitor::~Monitor() noexcept
        {
            delete mDebouncer;

            try
            {
                mLogger.LogDebug() << "Monitor destroyed";
            }
            catch (...)
            {
            }
        }
    }
}