#include <gtest/gtest.h>
#include <vector>
#include "./dummy_supervision.h"
#include "../../../../src/ara/phm/supervisors/global_supervision.h"

namespace ara
{
    namespace phm
    {
        namespace supervisors
        {
            TEST(GlobalSupervisionTest, EmptyConstructor)
            {
                const SupervisionStatus cExpectedResult{SupervisionStatus::kDeactivated};

                const GlobalSupervision cSupervision({});
                const SupervisionStatus cActualResult{cSupervision.GetStatus()};

                EXPECT_EQ(cExpectedResult, cActualResult);
            }

            TEST(GlobalSupervisionTest, Constructor)
            {
                const SupervisionStatus cExpectedResult{SupervisionStatus::kOk};

                DummySupervision _okSupervision;
                _okSupervision.ReportStatus(cExpectedResult);

                const GlobalSupervision cGlobalSupervision({&_okSupervision});
                const SupervisionStatus cActualResult{cGlobalSupervision.GetStatus()};

                EXPECT_EQ(cExpectedResult, cActualResult);
            }

            TEST(GlobalSupervisionTest, MutipleSupervisionsScenario)
            {
                DummySupervision _okSupervision;
                DummySupervision _failedSupervision;
                DummySupervision _expiredSupervision;

                const GlobalSupervision cGlobalSupervision(
                    {&_okSupervision, &_failedSupervision, &_expiredSupervision});

                SupervisionStatus _expectedResult{SupervisionStatus::kOk};
                _okSupervision.ReportStatus(_expectedResult);
                SupervisionStatus _actualResult{cGlobalSupervision.GetStatus()};
                EXPECT_EQ(_expectedResult, _actualResult);

                _expectedResult = SupervisionStatus::kFailed;
                _failedSupervision.ReportStatus(_expectedResult);
                _actualResult = cGlobalSupervision.GetStatus();
                EXPECT_EQ(_expectedResult, _actualResult);

                _expectedResult = SupervisionStatus::kExpired;
                _expiredSupervision.ReportStatus(_expectedResult);
                _actualResult = cGlobalSupervision.GetStatus();
                EXPECT_EQ(_expectedResult, _actualResult);
            }

            class TypedSupervision : public ElementarySupervision
            {
            public:
                explicit TypedSupervision(
                    TypeOfSupervision supervisionType) noexcept : ElementarySupervision(supervisionType)
                {
                }

                void ReportStatus(SupervisionStatus status)
                {
                    Report(status);
                }
            };

            TEST(GlobalSupervisionTest, ConstructorTakesWorstStatus)
            {
                const SupervisionStatus cExpectedResult{SupervisionStatus::kExpired};

                DummySupervision _okSupervision;
                DummySupervision _expiredSupervision;
                DummySupervision _failedSupervision;
                _okSupervision.ReportStatus(SupervisionStatus::kOk);
                _expiredSupervision.ReportStatus(SupervisionStatus::kExpired);
                _failedSupervision.ReportStatus(SupervisionStatus::kFailed);

                const GlobalSupervision cGlobalSupervision(
                    {&_okSupervision, &_expiredSupervision, &_failedSupervision});

                EXPECT_EQ(cExpectedResult, cGlobalSupervision.GetStatus());
            }

            TEST(GlobalSupervisionTest, DeactivatedSupervisionsConstructor)
            {
                const SupervisionStatus cExpectedResult{SupervisionStatus::kDeactivated};

                DummySupervision _firstSupervision;
                DummySupervision _secondSupervision;

                const GlobalSupervision cGlobalSupervision(
                    {&_firstSupervision, &_secondSupervision});

                EXPECT_EQ(cExpectedResult, cGlobalSupervision.GetStatus());
            }

            TEST(GlobalSupervisionTest, SetCallbackMethod)
            {
                const SupervisionStatus cExpectedStatus{SupervisionStatus::kFailed};
                const TypeOfSupervision cExpectedType{
                    TypeOfSupervision::DeadlineSupervision};

                TypedSupervision _aliveSupervision(TypeOfSupervision::AliveSupervision);
                TypedSupervision _deadlineSupervision(TypeOfSupervision::DeadlineSupervision);
                GlobalSupervision _globalSupervision(
                    {&_aliveSupervision, &_deadlineSupervision});

                std::vector<SupervisionUpdate> _updates;
                _globalSupervision.SetCallback(
                    [&_updates](SupervisionUpdate update)
                    { _updates.push_back(update); });

                _deadlineSupervision.ReportStatus(cExpectedStatus);

                ASSERT_EQ(1U, _updates.size());
                EXPECT_EQ(cExpectedStatus, _updates[0].status);
                EXPECT_EQ(cExpectedType, _updates[0].type);
                EXPECT_EQ(cExpectedStatus, _globalSupervision.GetStatus());
            }

            TEST(GlobalSupervisionTest, NoCallbackOnUnchangedGlobalStatus)
            {
                DummySupervision _firstSupervision;
                DummySupervision _secondSupervision;
                GlobalSupervision _globalSupervision(
                    {&_firstSupervision, &_secondSupervision});

                uint32_t _counter{0};
                _globalSupervision.SetCallback(
                    [&_counter](SupervisionUpdate update)
                    { ++_counter; });

                _firstSupervision.ReportStatus(SupervisionStatus::kFailed);
                EXPECT_EQ(1U, _counter);

                // The global status is dominated by the failed supervision
                _secondSupervision.ReportStatus(SupervisionStatus::kOk);
                EXPECT_EQ(1U, _counter);

                _secondSupervision.ReportStatus(SupervisionStatus::kFailed);
                EXPECT_EQ(1U, _counter);
                EXPECT_EQ(SupervisionStatus::kFailed, _globalSupervision.GetStatus());
            }

            TEST(GlobalSupervisionTest, DominantSupervisionTypeScenario)
            {
                TypedSupervision _aliveSupervision(TypeOfSupervision::AliveSupervision);
                TypedSupervision _deadlineSupervision(TypeOfSupervision::DeadlineSupervision);
                TypedSupervision _logicalSupervision(TypeOfSupervision::LogicalSupervision);
                GlobalSupervision _globalSupervision(
                    {&_aliveSupervision, &_deadlineSupervision, &_logicalSupervision});

                std::vector<SupervisionUpdate> _updates;
                _globalSupervision.SetCallback(
                    [&_updates](SupervisionUpdate update)
                    { _updates.push_back(update); });

                _logicalSupervision.ReportStatus(SupervisionStatus::kOk);
                _deadlineSupervision.ReportStatus(SupervisionStatus::kFailed);
                _aliveSupervision.ReportStatus(SupervisionStatus::kExpired);

                ASSERT_EQ(3U, _updates.size());
                EXPECT_EQ(SupervisionStatus::kOk, _updates[0].status);
                EXPECT_EQ(TypeOfSupervision::LogicalSupervision, _updates[0].type);
                EXPECT_EQ(SupervisionStatus::kFailed, _updates[1].status);
                EXPECT_EQ(TypeOfSupervision::DeadlineSupervision, _updates[1].type);
                EXPECT_EQ(SupervisionStatus::kExpired, _updates[2].status);
                EXPECT_EQ(TypeOfSupervision::AliveSupervision, _updates[2].type);
            }

            TEST(GlobalSupervisionTest, FirstSupervisionWinsOnEqualStatus)
            {
                const SupervisionStatus cExpectedStatus{SupervisionStatus::kFailed};
                const TypeOfSupervision cExpectedType{TypeOfSupervision::AliveSupervision};

                TypedSupervision _aliveSupervision(TypeOfSupervision::AliveSupervision);
                TypedSupervision _deadlineSupervision(TypeOfSupervision::DeadlineSupervision);
                TypedSupervision _logicalSupervision(TypeOfSupervision::LogicalSupervision);
                _aliveSupervision.ReportStatus(cExpectedStatus);
                _deadlineSupervision.ReportStatus(cExpectedStatus);
                _logicalSupervision.ReportStatus(SupervisionStatus::kExpired);
                GlobalSupervision _globalSupervision(
                    {&_aliveSupervision, &_deadlineSupervision, &_logicalSupervision});

                std::vector<SupervisionUpdate> _updates;
                _globalSupervision.SetCallback(
                    [&_updates](SupervisionUpdate update)
                    { _updates.push_back(update); });

                // Two equally failed supervisions remain after the deactivation
                _logicalSupervision.ReportStatus(SupervisionStatus::kDeactivated);

                ASSERT_EQ(1U, _updates.size());
                EXPECT_EQ(cExpectedStatus, _updates[0].status);
                EXPECT_EQ(cExpectedType, _updates[0].type);
            }

            TEST(GlobalSupervisionTest, RecoveryScenario)
            {
                TypedSupervision _aliveSupervision(TypeOfSupervision::AliveSupervision);
                TypedSupervision _deadlineSupervision(TypeOfSupervision::DeadlineSupervision);
                GlobalSupervision _globalSupervision(
                    {&_aliveSupervision, &_deadlineSupervision});

                std::vector<SupervisionUpdate> _updates;
                _globalSupervision.SetCallback(
                    [&_updates](SupervisionUpdate update)
                    { _updates.push_back(update); });

                _aliveSupervision.ReportStatus(SupervisionStatus::kFailed);
                _deadlineSupervision.ReportStatus(SupervisionStatus::kExpired);
                // Deactivating the expired supervision lowers the global status
                _deadlineSupervision.ReportStatus(SupervisionStatus::kDeactivated);
                _aliveSupervision.ReportStatus(SupervisionStatus::kOk);
                _aliveSupervision.ReportStatus(SupervisionStatus::kDeactivated);

                ASSERT_EQ(5U, _updates.size());
                EXPECT_EQ(SupervisionStatus::kFailed, _updates[0].status);
                EXPECT_EQ(SupervisionStatus::kExpired, _updates[1].status);
                EXPECT_EQ(TypeOfSupervision::DeadlineSupervision, _updates[1].type);
                EXPECT_EQ(SupervisionStatus::kFailed, _updates[2].status);
                EXPECT_EQ(TypeOfSupervision::AliveSupervision, _updates[2].type);
                EXPECT_EQ(SupervisionStatus::kOk, _updates[3].status);
                EXPECT_EQ(SupervisionStatus::kDeactivated, _updates[4].status);
                EXPECT_EQ(SupervisionStatus::kDeactivated, _globalSupervision.GetStatus());
            }

            TEST(GlobalSupervisionTest, StatusChangeWithoutCallback)
            {
                const SupervisionStatus cExpectedResult{SupervisionStatus::kFailed};

                DummySupervision _supervision;
                const GlobalSupervision cGlobalSupervision({&_supervision});

                EXPECT_NO_THROW(_supervision.ReportStatus(cExpectedResult));
                EXPECT_EQ(cExpectedResult, cGlobalSupervision.GetStatus());
            }
        }
    }
}
