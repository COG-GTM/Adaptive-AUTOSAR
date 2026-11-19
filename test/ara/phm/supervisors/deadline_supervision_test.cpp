#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include "./status_recorder.h"
#include "../../../../src/ara/phm/supervisors/deadline_supervision.h"

namespace ara
{
    namespace phm
    {
        namespace supervisors
        {
            static const int cNyquistMargin = 2;

            TEST(DeadlineSupervisionTest, InvalidMinDeadlineConstruction)
            {
                const std::chrono::milliseconds cMinDeadline{2};
                const std::chrono::milliseconds cMaxDeadline{2};

                EXPECT_THROW(
                    DeadlineSupervision _supervision(cMinDeadline, cMaxDeadline),
                    std::invalid_argument);
            }

            TEST(DeadlineSupervisionTest, ZeroMaxDeadlineConstruction)
            {
                const std::chrono::milliseconds cMinDeadline{-1};
                const std::chrono::milliseconds cMaxDeadline{0};

                EXPECT_THROW(
                    DeadlineSupervision _supervision(cMinDeadline, cMaxDeadline),
                    std::invalid_argument);
            }

            TEST(DeadlineSupervisionTest, DeadlinePassScenario)
            {
                const std::chrono::milliseconds cMinDeadline{2};
                const std::chrono::milliseconds cTargetReportTime{cMinDeadline * cNyquistMargin};
                const std::chrono::milliseconds cMaxDeadline{cTargetReportTime * cNyquistMargin};

                bool _failed{false};

                DeadlineSupervision _supervision(cMinDeadline, cMaxDeadline);

                _supervision.SetCallback([&](SupervisionStatus status)
                                         { _failed = status == SupervisionStatus::kExpired; });
                _supervision.ReportSourceCheckpoint();
                std::this_thread::sleep_for(cTargetReportTime);
                _supervision.ReportTargetCheckpoint();
                std::this_thread::sleep_for(cMaxDeadline * cNyquistMargin);

                EXPECT_FALSE(_failed);
            }

            TEST(DeadlineSupervisionTest, IgnoreTargetCheckpointScenario)
            {
                const std::chrono::milliseconds cMinDeadline{2};
                const std::chrono::milliseconds cMaxDeadline{3};

                bool _failed{false};

                DeadlineSupervision _supervision(cMinDeadline, cMaxDeadline);

                _supervision.SetCallback([&](SupervisionStatus status)
                                         { _failed = status == SupervisionStatus::kExpired; });
                _supervision.ReportTargetCheckpoint();

                EXPECT_FALSE(_failed);
            }

            TEST(DeadlineSupervisionTest, DoubleSourceCheckpointReportScenario)
            {
                const std::chrono::milliseconds cMinDeadline{2};
                const std::chrono::milliseconds cTargetReportTime{cMinDeadline * cNyquistMargin};
                const std::chrono::milliseconds cMaxDeadline{cTargetReportTime * cNyquistMargin};

                bool _failed{false};

                DeadlineSupervision _supervision(cMinDeadline, cMaxDeadline);

                _supervision.SetCallback([&](SupervisionStatus status)
                                         { _failed = status == SupervisionStatus::kExpired; });
                _supervision.ReportSourceCheckpoint();
                std::this_thread::sleep_for(cTargetReportTime);
                _supervision.ReportSourceCheckpoint();
                std::this_thread::sleep_for(cMaxDeadline * cNyquistMargin);

                EXPECT_TRUE(_failed);
            }

            TEST(DeadlineSupervisionTest, MinDeadlineFailScenario)
            {
                const std::chrono::milliseconds cMinDeadline{2};
                const std::chrono::milliseconds cMaxDeadline{3};

                bool _failed{false};

                DeadlineSupervision _supervision(cMinDeadline, cMaxDeadline);

                _supervision.SetCallback([&](SupervisionStatus status)
                                         { _failed = status == SupervisionStatus::kExpired; });
                _supervision.ReportSourceCheckpoint();
                _supervision.ReportTargetCheckpoint();

                std::this_thread::sleep_for(cMaxDeadline * cNyquistMargin);

                EXPECT_TRUE(_failed);
            }

            TEST(DeadlineSupervisionTest, MaxDeadlineFailScenario)
            {
                const std::chrono::milliseconds cMinDeadline{2};
                const std::chrono::milliseconds cMaxDeadline{3};

                bool _failed{false};

                DeadlineSupervision _supervision(cMinDeadline, cMaxDeadline);

                _supervision.SetCallback([&](SupervisionStatus status)
                                         { _failed = status == SupervisionStatus::kExpired; });
                _supervision.ReportSourceCheckpoint();
                std::this_thread::sleep_for(cMaxDeadline * cNyquistMargin);
                _supervision.ReportTargetCheckpoint();

                EXPECT_TRUE(_failed);
            }

            static const std::chrono::milliseconds cDeadlineTimeout{5000};

            TEST(DeadlineSupervisionTest, ValidConstruction)
            {
                const std::chrono::milliseconds cMinDeadline{2};
                const std::chrono::milliseconds cMaxDeadline{3};

                const DeadlineSupervision cSupervision(cMinDeadline, cMaxDeadline);

                EXPECT_EQ(SupervisionStatus::kDeactivated, cSupervision.GetStatus());
                EXPECT_EQ(TypeOfSupervision::DeadlineSupervision, cSupervision.GetType());
            }

            TEST(DeadlineSupervisionTest, MinDeadlineGreaterThanMaxDeadlineConstruction)
            {
                const std::chrono::milliseconds cMinDeadline{3};
                const std::chrono::milliseconds cMaxDeadline{2};

                EXPECT_THROW(
                    DeadlineSupervision _supervision(cMinDeadline, cMaxDeadline),
                    std::invalid_argument);
            }

            TEST(DeadlineSupervisionTest, NegativeMaxDeadlineConstruction)
            {
                const std::chrono::milliseconds cMinDeadline{-2};
                const std::chrono::milliseconds cMaxDeadline{-1};

                EXPECT_THROW(
                    DeadlineSupervision _supervision(cMinDeadline, cMaxDeadline),
                    std::invalid_argument);
            }

            TEST(DeadlineSupervisionTest, DeadlinePassReportsOkScenario)
            {
                const std::chrono::milliseconds cMinDeadline{10};
                const std::chrono::milliseconds cTargetReportTime{100};
                const std::chrono::milliseconds cMaxDeadline{2000};
                const std::vector<SupervisionStatus> cExpectedResults{
                    SupervisionStatus::kOk};

                StatusRecorder _recorder;
                DeadlineSupervision _supervision(cMinDeadline, cMaxDeadline);
                _supervision.SetCallback(
                    [&_recorder](SupervisionStatus status)
                    { _recorder.Record(status); });

                _supervision.ReportSourceCheckpoint();
                std::this_thread::sleep_for(cTargetReportTime);
                _supervision.ReportTargetCheckpoint();

                EXPECT_TRUE(_recorder.WaitForSize(1, cDeadlineTimeout));
                EXPECT_EQ(cExpectedResults, _recorder.GetStatuses());
            }

            TEST(DeadlineSupervisionTest, MissingTargetCheckpointScenario)
            {
                const std::chrono::milliseconds cMinDeadline{2};
                const std::chrono::milliseconds cMaxDeadline{20};
                const std::vector<SupervisionStatus> cExpectedResults{
                    SupervisionStatus::kExpired};

                StatusRecorder _recorder;
                DeadlineSupervision _supervision(cMinDeadline, cMaxDeadline);
                _supervision.SetCallback(
                    [&_recorder](SupervisionStatus status)
                    { _recorder.Record(status); });

                _supervision.ReportSourceCheckpoint();

                EXPECT_TRUE(_recorder.WaitForSize(1, cDeadlineTimeout));
                EXPECT_EQ(cExpectedResults, _recorder.GetStatuses());
                EXPECT_EQ(SupervisionStatus::kExpired, _supervision.GetStatus());
            }

            TEST(DeadlineSupervisionTest, EarlyTargetCheckpointScenario)
            {
                const std::chrono::milliseconds cMinDeadline{1000};
                const std::chrono::milliseconds cTargetReportTime{50};
                const std::chrono::milliseconds cMaxDeadline{2000};
                const std::vector<SupervisionStatus> cExpectedResults{
                    SupervisionStatus::kExpired};

                StatusRecorder _recorder;
                DeadlineSupervision _supervision(cMinDeadline, cMaxDeadline);
                _supervision.SetCallback(
                    [&_recorder](SupervisionStatus status)
                    { _recorder.Record(status); });

                _supervision.ReportSourceCheckpoint();
                std::this_thread::sleep_for(cTargetReportTime);
                _supervision.ReportTargetCheckpoint();

                EXPECT_TRUE(_recorder.WaitForSize(1, cDeadlineTimeout));
                EXPECT_EQ(cExpectedResults, _recorder.GetStatuses());
            }

            TEST(DeadlineSupervisionTest, DestructionWaitsForWorker)
            {
                const std::chrono::milliseconds cMinDeadline{2};
                const std::chrono::milliseconds cMaxDeadline{20};
                const std::vector<SupervisionStatus> cExpectedResults{
                    SupervisionStatus::kExpired};

                StatusRecorder _recorder;
                {
                    DeadlineSupervision _supervision(cMinDeadline, cMaxDeadline);
                    _supervision.SetCallback(
                        [&_recorder](SupervisionStatus status)
                        { _recorder.Record(status); });
                    _supervision.ReportSourceCheckpoint();
                }

                // The destructor joins the worker, so the expiry is already reported.
                EXPECT_EQ(cExpectedResults, _recorder.GetStatuses());
            }

            /// @note Documents a production bug (not fixed here on purpose):
            ///       DeadlineSupervision::ReportSourceCheckpoint only starts the worker thread
            ///       while mDeadlineThread is not joinable. A finished (but never joined) worker
            ///       is still joinable, so after the first source/target cycle every later
            ///       source checkpoint only notifies a condition variable nobody waits on.
            ///       The second cycle is therefore not supervised at all and a missing
            ///       target checkpoint is never reported as expired.
            TEST(DeadlineSupervisionTest, DISABLED_BugSecondCycleIsNotSupervised)
            {
                const std::chrono::milliseconds cMinDeadline{10};
                const std::chrono::milliseconds cTargetReportTime{100};
                const std::chrono::milliseconds cMaxDeadline{300};
                const std::chrono::milliseconds cSecondCycleTimeout{cMaxDeadline * 5};
                const std::vector<SupervisionStatus> cExpectedResults{
                    SupervisionStatus::kOk, SupervisionStatus::kExpired};

                StatusRecorder _recorder;
                DeadlineSupervision _supervision(cMinDeadline, cMaxDeadline);
                _supervision.SetCallback(
                    [&_recorder](SupervisionStatus status)
                    { _recorder.Record(status); });

                // First cycle within the deadlines
                _supervision.ReportSourceCheckpoint();
                std::this_thread::sleep_for(cTargetReportTime);
                _supervision.ReportTargetCheckpoint();
                ASSERT_TRUE(_recorder.WaitForSize(1, cDeadlineTimeout));

                // Second cycle without any target checkpoint, which should expire.
                _supervision.ReportSourceCheckpoint();

                EXPECT_TRUE(_recorder.WaitForSize(2, cSecondCycleTimeout));
                EXPECT_EQ(cExpectedResults, _recorder.GetStatuses());
            }

            /// @note Documents a production bug (not fixed here on purpose):
            ///       DeadlineSupervision::deadlineWorker waits on the condition variable without a predicate
            ///       and ReportTargetCheckpoint notifies without holding the mutex.
            ///       If the target checkpoint is reported before the freshly started worker begins to wait,
            ///       the notification is lost, the worker runs into the maximum deadline timeout
            ///       and reports kExpired although the target was reached in time.
            ///       The outcome depends on thread scheduling (the loss is the usual case).
            TEST(DeadlineSupervisionTest, DISABLED_BugImmediateTargetCheckpointNotificationIsLost)
            {
                const std::chrono::milliseconds cMinDeadline{0};
                const std::chrono::milliseconds cMaxDeadline{300};
                const std::vector<SupervisionStatus> cExpectedResults{
                    SupervisionStatus::kOk};

                StatusRecorder _recorder;
                DeadlineSupervision _supervision(cMinDeadline, cMaxDeadline);
                _supervision.SetCallback(
                    [&_recorder](SupervisionStatus status)
                    { _recorder.Record(status); });

                _supervision.ReportSourceCheckpoint();
                _supervision.ReportTargetCheckpoint();

                EXPECT_TRUE(_recorder.WaitForSize(1, cDeadlineTimeout));
                EXPECT_EQ(cExpectedResults, _recorder.GetStatuses());
            }
        }
    }
}
