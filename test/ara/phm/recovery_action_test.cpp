#include <gtest/gtest.h>
#include <utility>
#include "../../../src/ara/phm/recovery_action.h"

namespace ara
{
    namespace phm
    {
        static const core::InstanceSpecifier cSpecifier("Instance0");

        class RecoveryActionTest : public RecoveryAction, public testing::Test
        {
        public:
            RecoveryActionTest() : RecoveryAction(cSpecifier)
            {
            }

            void RecoveryHandler(
                const exec::ExecutionErrorEvent &executionError,
                TypeOfSupervision supervision) override
            {
                // Empty body
            }
        };

        TEST_F(RecoveryActionTest, Constructor)
        {
            EXPECT_FALSE(IsOffered());
        }

        TEST_F(RecoveryActionTest, OfferMethod)
        {
            const core::Result<void> cResult{Offer()};
            EXPECT_TRUE(cResult.HasValue());
            EXPECT_TRUE(IsOffered());
        }

        TEST_F(RecoveryActionTest, StopOfferMethod)
        {
            Offer();
            StopOffer();
            EXPECT_FALSE(IsOffered());
        }

        class RecordingRecoveryAction : public RecoveryAction
        {
        private:
            uint32_t mInvocationCounter;
            exec::ExecutionError mLastExecutionError;
            TypeOfSupervision mLastSupervision;

        public:
            RecordingRecoveryAction() : RecoveryAction(cSpecifier),
                                        mInvocationCounter{0},
                                        mLastExecutionError{0},
                                        mLastSupervision{TypeOfSupervision::AliveSupervision}
            {
            }

            RecordingRecoveryAction(
                RecordingRecoveryAction &&other) noexcept : RecoveryAction(std::move(other)),
                                                            mInvocationCounter{other.mInvocationCounter},
                                                            mLastExecutionError{other.mLastExecutionError},
                                                            mLastSupervision{other.mLastSupervision}
            {
            }

            void RecoveryHandler(
                const exec::ExecutionErrorEvent &executionError,
                TypeOfSupervision supervision) override
            {
                ++mInvocationCounter;
                mLastExecutionError = executionError.executionError;
                mLastSupervision = supervision;
            }

            bool GetOffered() const noexcept
            {
                return IsOffered();
            }

            uint32_t GetInvocationCounter() const noexcept
            {
                return mInvocationCounter;
            }

            exec::ExecutionError GetLastExecutionError() const noexcept
            {
                return mLastExecutionError;
            }

            TypeOfSupervision GetLastSupervision() const noexcept
            {
                return mLastSupervision;
            }
        };

        TEST_F(RecoveryActionTest, RepeatedOfferScenario)
        {
            const core::Result<void> cFirstResult{Offer()};
            const core::Result<void> cSecondResult{Offer()};

            EXPECT_TRUE(cFirstResult.HasValue());
            EXPECT_TRUE(cSecondResult.HasValue());
            EXPECT_TRUE(IsOffered());
        }

        TEST_F(RecoveryActionTest, StopOfferWithoutOfferScenario)
        {
            StopOffer();
            EXPECT_FALSE(IsOffered());

            StopOffer();
            EXPECT_FALSE(IsOffered());
        }

        TEST_F(RecoveryActionTest, ReofferScenario)
        {
            Offer();
            StopOffer();
            const core::Result<void> cResult{Offer()};

            EXPECT_TRUE(cResult.HasValue());
            EXPECT_TRUE(IsOffered());
        }

        TEST(RecoveryActionMoveTest, MoveConstructorOfOfferedAction)
        {
            RecordingRecoveryAction _action;
            _action.Offer();

            const RecordingRecoveryAction cMovedAction{std::move(_action)};

            EXPECT_TRUE(cMovedAction.GetOffered());
        }

        TEST(RecoveryActionMoveTest, MoveConstructorOfNotOfferedAction)
        {
            RecordingRecoveryAction _action;

            RecordingRecoveryAction _movedAction{std::move(_action)};
            EXPECT_FALSE(_movedAction.GetOffered());

            // The moved action should be still functional
            const core::Result<void> cResult{_movedAction.Offer()};
            EXPECT_TRUE(cResult.HasValue());
            EXPECT_TRUE(_movedAction.GetOffered());
        }

        TEST(RecoveryActionHandlerTest, RecoveryHandlerMethod)
        {
            const exec::ExecutionError cExpectedError{0x1234};
            const TypeOfSupervision cExpectedSupervision{
                TypeOfSupervision::DeadlineSupervision};
            const exec::ExecutionErrorEvent cEvent{cExpectedError, nullptr};

            RecordingRecoveryAction _action;
            RecoveryAction &_baseAction{_action};
            _baseAction.Offer();
            _baseAction.RecoveryHandler(cEvent, cExpectedSupervision);

            EXPECT_EQ(1U, _action.GetInvocationCounter());
            EXPECT_EQ(cExpectedError, _action.GetLastExecutionError());
            EXPECT_EQ(cExpectedSupervision, _action.GetLastSupervision());
        }

        TEST(TypeOfSupervisionTest, UnderlyingValues)
        {
            EXPECT_EQ(0U, static_cast<uint32_t>(TypeOfSupervision::AliveSupervision));
            EXPECT_EQ(1U, static_cast<uint32_t>(TypeOfSupervision::DeadlineSupervision));
            EXPECT_EQ(2U, static_cast<uint32_t>(TypeOfSupervision::LogicalSupervision));
        }
    }
}
