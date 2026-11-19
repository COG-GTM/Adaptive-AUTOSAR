#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include "./status_recorder.h"
#include "../../../../src/ara/phm/supervisors/alive_supervision.h"

namespace ara
{
    namespace phm
    {
        namespace supervisors
        {
            TEST(AliveSupervisionTest, ZeroMinMarginConstruction)
            {
                const uint16_t cExpectedAliveIndications{2};
                const uint16_t cMinMargin{0};
                const uint16_t cMaxMargin{1};
                const std::chrono::milliseconds cAliveReferenceCycle{1};
                const uint8_t cFailedReferenceCyclesTolerance{1};

                EXPECT_THROW(
                    AliveSupervision _supervision(
                        cExpectedAliveIndications,
                        cMinMargin,
                        cMaxMargin,
                        cAliveReferenceCycle,
                        cFailedReferenceCyclesTolerance),
                    std::invalid_argument);
            }

            TEST(AliveSupervisionTest, BigMinMarginConstruction)
            {
                const uint16_t cExpectedAliveIndications{2};
                const uint16_t cMinMargin{2};
                const uint16_t cMaxMargin{1};
                const std::chrono::milliseconds cAliveReferenceCycle{1};
                const uint8_t cFailedReferenceCyclesTolerance{1};

                EXPECT_THROW(
                    AliveSupervision _supervision(
                        cExpectedAliveIndications,
                        cMinMargin,
                        cMaxMargin,
                        cAliveReferenceCycle,
                        cFailedReferenceCyclesTolerance),
                    std::invalid_argument);
            }

            TEST(AliveSupervisionTest, ZeroMaxMarginConstruction)
            {
                const uint16_t cExpectedAliveIndications{2};
                const uint16_t cMinMargin{1};
                const uint16_t cMaxMargin{0};
                const std::chrono::milliseconds cAliveReferenceCycle{1};
                const uint8_t cFailedReferenceCyclesTolerance{1};

                EXPECT_THROW(
                    AliveSupervision _supervision(
                        cExpectedAliveIndications,
                        cMinMargin,
                        cMaxMargin,
                        cAliveReferenceCycle,
                        cFailedReferenceCyclesTolerance),
                    std::invalid_argument);
            }

            TEST(AliveSupervisionTest, ZeroAliveReferenceCycleConstruction)
            {
                const uint16_t cExpectedAliveIndications{2};
                const uint16_t cMinMargin{1};
                const uint16_t cMaxMargin{1};
                const std::chrono::milliseconds cAliveReferenceCycle{0};
                const uint8_t cFailedReferenceCyclesTolerance{1};

                EXPECT_THROW(
                    AliveSupervision _supervision(
                        cExpectedAliveIndications,
                        cMinMargin,
                        cMaxMargin,
                        cAliveReferenceCycle,
                        cFailedReferenceCyclesTolerance),
                    std::invalid_argument);
            }

            TEST(AliveSupervisionTest, ZeroFailedReferenceCyclesToleranceConstruction)
            {
                const uint16_t cExpectedAliveIndications{2};
                const uint16_t cMinMargin{1};
                const uint16_t cMaxMargin{1};
                const std::chrono::milliseconds cAliveReferenceCycle{1};
                const uint8_t cFailedReferenceCyclesTolerance{0};

                EXPECT_THROW(
                    AliveSupervision _supervision(
                        cExpectedAliveIndications,
                        cMinMargin,
                        cMaxMargin,
                        cAliveReferenceCycle,
                        cFailedReferenceCyclesTolerance),
                    std::invalid_argument);
            }

            static const std::chrono::milliseconds cAliveTimeout{5000};

            TEST(AliveSupervisionTest, ValidConstruction)
            {
                const uint16_t cExpectedAliveIndications{2};
                const uint16_t cMinMargin{1};
                const uint16_t cMaxMargin{1};
                const std::chrono::milliseconds cAliveReferenceCycle{1};
                const uint8_t cFailedReferenceCyclesTolerance{1};

                const AliveSupervision cSupervision(
                    cExpectedAliveIndications,
                    cMinMargin,
                    cMaxMargin,
                    cAliveReferenceCycle,
                    cFailedReferenceCyclesTolerance);

                EXPECT_EQ(TypeOfSupervision::AliveSupervision, cSupervision.GetType());
            }

            TEST(AliveSupervisionTest, NegativeAliveReferenceCycleConstruction)
            {
                const uint16_t cExpectedAliveIndications{2};
                const uint16_t cMinMargin{1};
                const uint16_t cMaxMargin{1};
                const std::chrono::milliseconds cAliveReferenceCycle{-1};
                const uint8_t cFailedReferenceCyclesTolerance{1};

                EXPECT_THROW(
                    AliveSupervision _supervision(
                        cExpectedAliveIndications,
                        cMinMargin,
                        cMaxMargin,
                        cAliveReferenceCycle,
                        cFailedReferenceCyclesTolerance),
                    std::invalid_argument);
            }

            TEST(AliveSupervisionTest, AlivePassScenario)
            {
                const uint16_t cExpectedAliveIndications{2};
                const uint16_t cMinMargin{1};
                const uint16_t cMaxMargin{1};
                const std::chrono::milliseconds cAliveReferenceCycle{200};
                const uint8_t cFailedReferenceCyclesTolerance{1};
                const SupervisionStatus cExpectedResult{SupervisionStatus::kOk};

                StatusRecorder _recorder;
                AliveSupervision _supervision(
                    cExpectedAliveIndications,
                    cMinMargin,
                    cMaxMargin,
                    cAliveReferenceCycle,
                    cFailedReferenceCyclesTolerance);
                _supervision.SetCallback(
                    [&_recorder](SupervisionStatus status)
                    { _recorder.Record(status); });

                _supervision.ReportAliveness();
                _supervision.ReportAliveness();

                ASSERT_TRUE(_recorder.WaitForSize(1, cAliveTimeout));
                const std::vector<SupervisionStatus> cActualResults{
                    _recorder.GetStatuses()};
                EXPECT_EQ(cExpectedResult, cActualResults.front());
            }

            TEST(AliveSupervisionTest, MissingIndicationsExpiryScenario)
            {
                const uint16_t cExpectedAliveIndications{2};
                const uint16_t cMinMargin{1};
                const uint16_t cMaxMargin{1};
                const std::chrono::milliseconds cAliveReferenceCycle{10};
                const uint8_t cFailedReferenceCyclesTolerance{1};
                const std::vector<SupervisionStatus> cExpectedResults{
                    SupervisionStatus::kExpired};

                StatusRecorder _recorder;
                AliveSupervision _supervision(
                    cExpectedAliveIndications,
                    cMinMargin,
                    cMaxMargin,
                    cAliveReferenceCycle,
                    cFailedReferenceCyclesTolerance);
                _supervision.SetCallback(
                    [&_recorder](SupervisionStatus status)
                    { _recorder.Record(status); });

                EXPECT_TRUE(_recorder.WaitForSize(1, cAliveTimeout));
                EXPECT_EQ(cExpectedResults, _recorder.GetStatuses());
                EXPECT_EQ(SupervisionStatus::kExpired, _supervision.GetStatus());
            }

            TEST(AliveSupervisionTest, FailedBeforeExpiryScenario)
            {
                const uint16_t cExpectedAliveIndications{2};
                const uint16_t cMinMargin{1};
                const uint16_t cMaxMargin{1};
                const std::chrono::milliseconds cAliveReferenceCycle{10};
                const uint8_t cFailedReferenceCyclesTolerance{3};
                const std::vector<SupervisionStatus> cExpectedResults{
                    SupervisionStatus::kFailed, SupervisionStatus::kExpired};

                StatusRecorder _recorder;
                AliveSupervision _supervision(
                    cExpectedAliveIndications,
                    cMinMargin,
                    cMaxMargin,
                    cAliveReferenceCycle,
                    cFailedReferenceCyclesTolerance);
                _supervision.SetCallback(
                    [&_recorder](SupervisionStatus status)
                    { _recorder.Record(status); });

                // No aliveness is reported, hence the supervision fails and then expires.
                EXPECT_TRUE(_recorder.WaitForSize(2, cAliveTimeout));
                EXPECT_EQ(cExpectedResults, _recorder.GetStatuses());
            }

            TEST(AliveSupervisionTest, TooManyIndicationsScenario)
            {
                const uint16_t cExpectedAliveIndications{2};
                const uint16_t cMinMargin{1};
                const uint16_t cMaxMargin{1};
                const std::chrono::milliseconds cAliveReferenceCycle{200};
                const uint8_t cFailedReferenceCyclesTolerance{1};
                const uint16_t cIndications{4};
                const std::vector<SupervisionStatus> cExpectedResults{
                    SupervisionStatus::kExpired};

                StatusRecorder _recorder;
                AliveSupervision _supervision(
                    cExpectedAliveIndications,
                    cMinMargin,
                    cMaxMargin,
                    cAliveReferenceCycle,
                    cFailedReferenceCyclesTolerance);
                _supervision.SetCallback(
                    [&_recorder](SupervisionStatus status)
                    { _recorder.Record(status); });

                for (uint16_t i = 0; i < cIndications; ++i)
                {
                    _supervision.ReportAliveness();
                }

                EXPECT_TRUE(_recorder.WaitForSize(1, cAliveTimeout));
                EXPECT_EQ(cExpectedResults, _recorder.GetStatuses());
            }

            TEST(AliveSupervisionTest, ExpiryIsFinalScenario)
            {
                const uint16_t cExpectedAliveIndications{2};
                const uint16_t cMinMargin{1};
                const uint16_t cMaxMargin{1};
                const std::chrono::milliseconds cAliveReferenceCycle{10};
                const uint8_t cFailedReferenceCyclesTolerance{1};
                const int cWaitingCycles{5};
                const std::vector<SupervisionStatus> cExpectedResults{
                    SupervisionStatus::kExpired};

                StatusRecorder _recorder;
                AliveSupervision _supervision(
                    cExpectedAliveIndications,
                    cMinMargin,
                    cMaxMargin,
                    cAliveReferenceCycle,
                    cFailedReferenceCyclesTolerance);
                _supervision.SetCallback(
                    [&_recorder](SupervisionStatus status)
                    { _recorder.Record(status); });

                ASSERT_TRUE(_recorder.WaitForSize(1, cAliveTimeout));

                // Valid aliveness reports should not revive the expired supervision.
                _supervision.ReportAliveness();
                _supervision.ReportAliveness();
                std::this_thread::sleep_for(cAliveReferenceCycle * cWaitingCycles);

                EXPECT_EQ(cExpectedResults, _recorder.GetStatuses());
                EXPECT_EQ(SupervisionStatus::kExpired, _supervision.GetStatus());
            }

            TEST(AliveSupervisionTest, RecoveryFromFailedScenario)
            {
                const uint16_t cExpectedAliveIndications{100};
                const uint16_t cMinMargin{99};
                const uint16_t cMaxMargin{1000};
                const std::chrono::milliseconds cAliveReferenceCycle{50};
                const uint8_t cFailedReferenceCyclesTolerance{255};
                const std::chrono::milliseconds cReportPeriod{5};
                const std::vector<SupervisionStatus> cExpectedResults{
                    SupervisionStatus::kFailed, SupervisionStatus::kOk};

                StatusRecorder _recorder;
                AliveSupervision _supervision(
                    cExpectedAliveIndications,
                    cMinMargin,
                    cMaxMargin,
                    cAliveReferenceCycle,
                    cFailedReferenceCyclesTolerance);
                _supervision.SetCallback(
                    [&_recorder](SupervisionStatus status)
                    { _recorder.Record(status); });

                // No aliveness is reported at first, hence the supervision fails.
                ASSERT_TRUE(_recorder.WaitForSize(1, cAliveTimeout));

                // Then the aliveness is reported periodically until the supervision recovers.
                const std::chrono::steady_clock::time_point cDeadline{
                    std::chrono::steady_clock::now() + cAliveTimeout};
                while (_recorder.GetStatuses().size() < cExpectedResults.size() &&
                       std::chrono::steady_clock::now() < cDeadline)
                {
                    _supervision.ReportAliveness();
                    std::this_thread::sleep_for(cReportPeriod);
                }

                std::vector<SupervisionStatus> _actualResults{
                    _recorder.GetStatuses()};
                ASSERT_GE(_actualResults.size(), cExpectedResults.size());
                // Only the transitions up to the recovery are relevant
                _actualResults.resize(cExpectedResults.size());
                EXPECT_EQ(cExpectedResults, _actualResults);
            }

            TEST(AliveSupervisionTest, IndicationCounterSaturationScenario)
            {
                const uint16_t cExpectedAliveIndications{65534};
                const uint16_t cMinMargin{1};
                const uint16_t cMaxMargin{1};
                const std::chrono::milliseconds cAliveReferenceCycle{500};
                const uint8_t cFailedReferenceCyclesTolerance{1};
                const uint32_t cIndications{70000};
                const SupervisionStatus cExpectedResult{SupervisionStatus::kOk};

                StatusRecorder _recorder;
                AliveSupervision _supervision(
                    cExpectedAliveIndications,
                    cMinMargin,
                    cMaxMargin,
                    cAliveReferenceCycle,
                    cFailedReferenceCyclesTolerance);
                _supervision.SetCallback(
                    [&_recorder](SupervisionStatus status)
                    { _recorder.Record(status); });

                // The indication counter should saturate at 65535 instead of wrapping to zero.
                for (uint32_t i = 0; i < cIndications; ++i)
                {
                    _supervision.ReportAliveness();
                }

                ASSERT_TRUE(_recorder.WaitForSize(1, cAliveTimeout));
                const std::vector<SupervisionStatus> cActualResults{
                    _recorder.GetStatuses()};
                EXPECT_EQ(cExpectedResult, cActualResults.front());
            }

            TEST(AliveSupervisionTest, DestructionWithoutCallback)
            {
                const uint16_t cExpectedAliveIndications{2};
                const uint16_t cMinMargin{1};
                const uint16_t cMaxMargin{1};
                const std::chrono::milliseconds cAliveReferenceCycle{5};
                const uint8_t cFailedReferenceCyclesTolerance{2};
                const int cWaitingCycles{40};

                {
                    AliveSupervision _supervision(
                        cExpectedAliveIndications,
                        cMinMargin,
                        cMaxMargin,
                        cAliveReferenceCycle,
                        cFailedReferenceCyclesTolerance);
                    std::this_thread::sleep_for(cAliveReferenceCycle * cWaitingCycles);
                    EXPECT_EQ(SupervisionStatus::kExpired, _supervision.GetStatus());
                }

                SUCCEED();
            }

            /// @note Documents a production bug (not fixed here on purpose):
            ///       AliveSupervision computes its upper bound as
            ///       static_cast<uint16_t>(expectedAliveIndications + maxMargin) without a range check.
            ///       For 65535 + 1 the bound wraps to 0, so no number of indications can satisfy
            ///       the supervision and it expires although exactly the expected number is reported.
            TEST(AliveSupervisionTest, DISABLED_BugMaxMarginOverflowWrapsUpperBound)
            {
                const uint16_t cExpectedAliveIndications{65535};
                const uint16_t cMinMargin{1};
                const uint16_t cMaxMargin{1};
                const std::chrono::milliseconds cAliveReferenceCycle{500};
                const uint8_t cFailedReferenceCyclesTolerance{1};
                const SupervisionStatus cExpectedResult{SupervisionStatus::kOk};

                StatusRecorder _recorder;
                AliveSupervision _supervision(
                    cExpectedAliveIndications,
                    cMinMargin,
                    cMaxMargin,
                    cAliveReferenceCycle,
                    cFailedReferenceCyclesTolerance);
                _supervision.SetCallback(
                    [&_recorder](SupervisionStatus status)
                    { _recorder.Record(status); });

                for (uint32_t i = 0; i < cExpectedAliveIndications; ++i)
                {
                    _supervision.ReportAliveness();
                }

                ASSERT_TRUE(_recorder.WaitForSize(1, cAliveTimeout));
                const std::vector<SupervisionStatus> cActualResults{
                    _recorder.GetStatuses()};
                EXPECT_EQ(cExpectedResult, cActualResults.front());
            }
        }
    }
}
