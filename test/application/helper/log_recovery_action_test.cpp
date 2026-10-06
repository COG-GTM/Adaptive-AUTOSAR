#include <gtest/gtest.h>
#include "../../../src/application/helper/log_recovery_action.h"
#include "./testable_fixture.h"

namespace application
{
    namespace helper
    {
        class LogRecoveryActionTest : public testing::Test
        {
        protected:
            const ara::exec::FunctionGroup cFunctionGroup{
                ara::exec::FunctionGroup::Create("MachineFG").Value()};
            LogRecoveryAction RecoveryAction;

            ara::exec::ExecutionErrorEvent GetErrorEvent(ara::exec::ExecutionError error) const
            {
                ara::exec::ExecutionErrorEvent _result;
                _result.executionError = error;
                _result.functionGroup = &cFunctionGroup;
                return _result;
            }
        };

        TEST_F(LogRecoveryActionTest, NotOfferedAction)
        {
            fixture::StdoutCapture _stdout;
            RecoveryAction.RecoveryHandler(
                GetErrorEvent(0), ara::phm::TypeOfSupervision::AliveSupervision);
            EXPECT_TRUE(_stdout.GetOutput().empty());
        }

        TEST_F(LogRecoveryActionTest, AliveSupervisionExpiration)
        {
            ASSERT_TRUE(RecoveryAction.Offer().HasValue());
            fixture::StdoutCapture _stdout;
            RecoveryAction.RecoveryHandler(
                GetErrorEvent(0), ara::phm::TypeOfSupervision::AliveSupervision);
            EXPECT_TRUE(_stdout.Contains("Alive supervision is expired on MachineFG"));
        }

        TEST_F(LogRecoveryActionTest, DeadlineSupervisionExpiration)
        {
            ASSERT_TRUE(RecoveryAction.Offer().HasValue());
            fixture::StdoutCapture _stdout;
            RecoveryAction.RecoveryHandler(
                GetErrorEvent(0), ara::phm::TypeOfSupervision::DeadlineSupervision);
            EXPECT_TRUE(_stdout.Contains("Deadline supervision is expired on MachineFG"));
        }

        TEST_F(LogRecoveryActionTest, UnhandledSupervisionType)
        {
            ASSERT_TRUE(RecoveryAction.Offer().HasValue());
            fixture::StdoutCapture _stdout;
            RecoveryAction.RecoveryHandler(
                GetErrorEvent(0), ara::phm::TypeOfSupervision::LogicalSupervision);
            EXPECT_TRUE(_stdout.GetOutput().empty());
        }

        TEST_F(LogRecoveryActionTest, UnhandledExecutionError)
        {
            ASSERT_TRUE(RecoveryAction.Offer().HasValue());
            fixture::StdoutCapture _stdout;
            RecoveryAction.RecoveryHandler(
                GetErrorEvent(1), ara::phm::TypeOfSupervision::AliveSupervision);
            EXPECT_TRUE(_stdout.GetOutput().empty());
        }

        TEST_F(LogRecoveryActionTest, StoppedOffer)
        {
            ASSERT_TRUE(RecoveryAction.Offer().HasValue());
            RecoveryAction.StopOffer();
            fixture::StdoutCapture _stdout;
            RecoveryAction.RecoveryHandler(
                GetErrorEvent(0), ara::phm::TypeOfSupervision::DeadlineSupervision);
            EXPECT_TRUE(_stdout.GetOutput().empty());
        }
    }
}
