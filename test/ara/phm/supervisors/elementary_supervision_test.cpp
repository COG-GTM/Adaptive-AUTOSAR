#include <gtest/gtest.h>
#include <stdexcept>
#include <vector>
#include "./dummy_supervision.h"

namespace ara
{
    namespace phm
    {
        namespace supervisors
        {
            TEST(ElementarySupervisionTest, Constructor)
            {
                const SupervisionStatus cExpectedResult{SupervisionStatus::kDeactivated};

                const DummySupervision cSupervision;
                const SupervisionStatus cActualResult{cSupervision.GetStatus()};

                EXPECT_EQ(cExpectedResult, cActualResult);
            }

            TEST(ElementarySupervisionTest, TransitionToOkScenario)
            {
                const SupervisionStatus cExpectedResult{SupervisionStatus::kOk};

                DummySupervision _supervision;

                _supervision.ReportStatus(cExpectedResult);
                SupervisionStatus _actualResult{_supervision.GetStatus()};
                EXPECT_EQ(cExpectedResult, _actualResult);

                // Transit to Failed and then to OK
                _supervision.ReportStatus(SupervisionStatus::kFailed);
                _supervision.ReportStatus(cExpectedResult);
                _actualResult = _supervision.GetStatus();
                EXPECT_EQ(cExpectedResult, _actualResult);

                // Transit to Expired and then to OK
                _supervision.ReportStatus(SupervisionStatus::kExpired);
                _supervision.ReportStatus(cExpectedResult);
                _actualResult = _supervision.GetStatus();
                EXPECT_NE(cExpectedResult, _actualResult);
            }

            TEST(ElementarySupervisionTest, TransitionToFailedScenario)
            {
                const SupervisionStatus cExpectedResult{SupervisionStatus::kFailed};

                DummySupervision _supervision;

                _supervision.ReportStatus(cExpectedResult);
                SupervisionStatus _actualResult{_supervision.GetStatus()};
                EXPECT_EQ(cExpectedResult, _actualResult);

                // Transit to OK and then to Failed
                _supervision.ReportStatus(SupervisionStatus::kOk);
                _supervision.ReportStatus(cExpectedResult);
                _actualResult = _supervision.GetStatus();
                EXPECT_EQ(cExpectedResult, _actualResult);

                // Transit to Expired and then to Failed
                _supervision.ReportStatus(SupervisionStatus::kExpired);
                _supervision.ReportStatus(cExpectedResult);
                _actualResult = _supervision.GetStatus();
                EXPECT_NE(cExpectedResult, _actualResult);
            }

            TEST(ElementarySupervisionTest, TransitionToExpiredScenario)
            {
                const SupervisionStatus cExpectedResult{SupervisionStatus::kExpired};

                DummySupervision _supervision;

                _supervision.ReportStatus(cExpectedResult);
                SupervisionStatus _actualResult{_supervision.GetStatus()};
                EXPECT_EQ(cExpectedResult, _actualResult);

                // Transit to OK and then to Expired
                _supervision.ReportStatus(SupervisionStatus::kOk);
                _supervision.ReportStatus(cExpectedResult);
                _actualResult = _supervision.GetStatus();
                EXPECT_EQ(cExpectedResult, _actualResult);

                // Transit to Failed and then to Expired
                _supervision.ReportStatus(SupervisionStatus::kFailed);
                _supervision.ReportStatus(cExpectedResult);
                _actualResult = _supervision.GetStatus();
                EXPECT_EQ(cExpectedResult, _actualResult);
            }

            TEST(ElementarySupervisionTest, TransitionToDeactivatedScenario)
            {
                const SupervisionStatus cExpectedResult{SupervisionStatus::kDeactivated};

                DummySupervision _supervision;

                _supervision.ReportStatus(cExpectedResult);
                SupervisionStatus _actualResult{_supervision.GetStatus()};
                EXPECT_EQ(cExpectedResult, _actualResult);

                // Transit to OK and then to Deactivated
                _supervision.ReportStatus(SupervisionStatus::kOk);
                _supervision.ReportStatus(cExpectedResult);
                _actualResult = _supervision.GetStatus();
                EXPECT_EQ(cExpectedResult, _actualResult);

                // Transit to Failed and then to Deactivated
                _supervision.ReportStatus(SupervisionStatus::kFailed);
                _supervision.ReportStatus(cExpectedResult);
                _actualResult = _supervision.GetStatus();
                EXPECT_EQ(cExpectedResult, _actualResult);

                // Transit to Expired and then to Deactivated
                _supervision.ReportStatus(SupervisionStatus::kExpired);
                _supervision.ReportStatus(cExpectedResult);
                _actualResult = _supervision.GetStatus();
                EXPECT_EQ(cExpectedResult, _actualResult);
            }

            TEST(ElementarySupervisionTest, GetTypeMethod)
            {
                const TypeOfSupervision cExpectedResult{
                    TypeOfSupervision::LogicalSupervision};

                const DummySupervision cSupervision;
                const TypeOfSupervision cActualResult{cSupervision.GetType()};

                EXPECT_EQ(cExpectedResult, cActualResult);
            }

            TEST(ElementarySupervisionTest, CallbackOnStatusChange)
            {
                const std::vector<SupervisionStatus> cExpectedResults{
                    SupervisionStatus::kOk,
                    SupervisionStatus::kFailed,
                    SupervisionStatus::kExpired,
                    SupervisionStatus::kDeactivated};

                DummySupervision _supervision;
                std::vector<SupervisionStatus> _actualResults;
                _supervision.SetCallback(
                    [&_actualResults](SupervisionStatus status)
                    { _actualResults.push_back(status); });

                for (SupervisionStatus expectedResult : cExpectedResults)
                {
                    _supervision.ReportStatus(expectedResult);
                }

                EXPECT_EQ(cExpectedResults, _actualResults);
            }

            TEST(ElementarySupervisionTest, NoCallbackOnSameStatus)
            {
                DummySupervision _supervision;
                uint32_t _counter{0};
                _supervision.SetCallback(
                    [&_counter](SupervisionStatus status)
                    { ++_counter; });

                // The initial status is already deactivated
                _supervision.ReportStatus(SupervisionStatus::kDeactivated);
                EXPECT_EQ(0U, _counter);

                _supervision.ReportStatus(SupervisionStatus::kOk);
                _supervision.ReportStatus(SupervisionStatus::kOk);
                EXPECT_EQ(1U, _counter);

                _supervision.ReportStatus(SupervisionStatus::kFailed);
                _supervision.ReportStatus(SupervisionStatus::kFailed);
                EXPECT_EQ(2U, _counter);

                _supervision.ReportStatus(SupervisionStatus::kExpired);
                _supervision.ReportStatus(SupervisionStatus::kExpired);
                EXPECT_EQ(3U, _counter);
            }

            TEST(ElementarySupervisionTest, NoCallbackOnRejectedTransitionFromExpired)
            {
                const SupervisionStatus cExpectedResult{SupervisionStatus::kExpired};

                DummySupervision _supervision;
                _supervision.ReportStatus(cExpectedResult);

                uint32_t _counter{0};
                _supervision.SetCallback(
                    [&_counter](SupervisionStatus status)
                    { ++_counter; });

                _supervision.ReportStatus(SupervisionStatus::kOk);
                _supervision.ReportStatus(SupervisionStatus::kFailed);

                EXPECT_EQ(0U, _counter);
                EXPECT_EQ(cExpectedResult, _supervision.GetStatus());
            }

            TEST(ElementarySupervisionTest, ReactivationAfterExpiryScenario)
            {
                const SupervisionStatus cExpectedResult{SupervisionStatus::kOk};

                DummySupervision _supervision;
                _supervision.ReportStatus(SupervisionStatus::kExpired);
                _supervision.ReportStatus(SupervisionStatus::kDeactivated);
                _supervision.ReportStatus(cExpectedResult);

                EXPECT_EQ(cExpectedResult, _supervision.GetStatus());
            }

            TEST(ElementarySupervisionTest, CallbackReplacement)
            {
                DummySupervision _supervision;
                uint32_t _firstCounter{0};
                uint32_t _secondCounter{0};
                _supervision.SetCallback(
                    [&_firstCounter](SupervisionStatus status)
                    { ++_firstCounter; });
                _supervision.SetCallback(
                    [&_secondCounter](SupervisionStatus status)
                    { ++_secondCounter; });

                _supervision.ReportStatus(SupervisionStatus::kOk);

                EXPECT_EQ(0U, _firstCounter);
                EXPECT_EQ(1U, _secondCounter);
            }

            TEST(ElementarySupervisionTest, EmptyCallbackScenario)
            {
                DummySupervision _supervision;
                _supervision.SetCallback(nullptr);

                EXPECT_NO_THROW(_supervision.ReportStatus(SupervisionStatus::kOk));
                EXPECT_NO_THROW(_supervision.ReportStatus(SupervisionStatus::kExpired));
                EXPECT_NO_THROW(_supervision.ReportStatus(SupervisionStatus::kDeactivated));
            }

            TEST(ElementarySupervisionTest, UnhandledStatusReport)
            {
                const uint8_t cUnhandledValue{4};
                const SupervisionStatus cUnhandledStatus{
                    static_cast<SupervisionStatus>(cUnhandledValue)};
                const SupervisionStatus cExpectedResult{SupervisionStatus::kOk};

                DummySupervision _supervision;
                _supervision.ReportStatus(cExpectedResult);

                uint32_t _counter{0};
                _supervision.SetCallback(
                    [&_counter](SupervisionStatus status)
                    { ++_counter; });

                EXPECT_THROW(
                    _supervision.ReportStatus(cUnhandledStatus), std::out_of_range);
                EXPECT_EQ(0U, _counter);
                EXPECT_EQ(cExpectedResult, _supervision.GetStatus());
            }
        }
    }
}
