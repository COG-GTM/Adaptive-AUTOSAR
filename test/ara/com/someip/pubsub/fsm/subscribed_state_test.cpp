#include <gtest/gtest.h>
#include "../../../../../../src/ara/com/helper/finite_state_machine.h"
#include "../../../../../../src/ara/com/someip/pubsub/fsm/service_down_state.h"
#include "../../../../../../src/ara/com/someip/pubsub/fsm/notsubscribed_state.h"
#include "../../../../../../src/ara/com/someip/pubsub/fsm/subscribed_state.h"

namespace ara
{
    namespace com
    {
        namespace someip
        {
            namespace pubsub
            {
                namespace fsm
                {
                    class SubscribedStateTest : public testing::Test
                    {
                    protected:
                        ServiceDownState ServiceDown;
                        NotSubscribedState NotSubscribed;
                        SubscribedState Subscribed;
                        helper::FiniteStateMachine<helper::PubSubState> StateMachine;

                        void Initialize(helper::PubSubState entrypoint)
                        {
                            StateMachine.Initialize(
                                {&ServiceDown, &NotSubscribed, &Subscribed},
                                entrypoint);
                        }
                    };

                    TEST_F(SubscribedStateTest, LastUnsubscription)
                    {
                        Initialize(helper::PubSubState::Subscribed);

                        Subscribed.Unsubscribed();

                        EXPECT_EQ(
                            helper::PubSubState::NotSubscribed,
                            StateMachine.GetState());
                    }

                    TEST_F(SubscribedStateTest, SubscriptionCounting)
                    {
                        Initialize(helper::PubSubState::Subscribed);

                        // Two subscribers in total
                        Subscribed.Subscribed();

                        Subscribed.Unsubscribed();
                        EXPECT_EQ(
                            helper::PubSubState::Subscribed,
                            StateMachine.GetState());

                        Subscribed.Unsubscribed();
                        EXPECT_EQ(
                            helper::PubSubState::NotSubscribed,
                            StateMachine.GetState());
                    }

                    TEST_F(SubscribedStateTest, StopWhileSubscribed)
                    {
                        Initialize(helper::PubSubState::Subscribed);

                        Subscribed.Subscribed();
                        Subscribed.Stopped();

                        EXPECT_EQ(
                            helper::PubSubState::ServiceDown,
                            StateMachine.GetState());
                    }

                    TEST_F(SubscribedStateTest, CounterResetOnReactivation)
                    {
                        Initialize(helper::PubSubState::Subscribed);

                        // Three subscribers, then the service goes down and up again
                        Subscribed.Subscribed();
                        Subscribed.Subscribed();
                        Subscribed.Stopped();
                        ServiceDown.Started();
                        EXPECT_EQ(
                            helper::PubSubState::NotSubscribed,
                            StateMachine.GetState());

                        NotSubscribed.Subscribed();
                        EXPECT_EQ(
                            helper::PubSubState::Subscribed,
                            StateMachine.GetState());

                        // A single unsubscription is enough after the re-activation
                        Subscribed.Unsubscribed();
                        EXPECT_EQ(
                            helper::PubSubState::NotSubscribed,
                            StateMachine.GetState());
                    }

                    TEST_F(SubscribedStateTest, FullLifecycle)
                    {
                        Initialize(helper::PubSubState::ServiceDown);

                        ServiceDown.Started();
                        EXPECT_EQ(
                            helper::PubSubState::NotSubscribed,
                            StateMachine.GetState());

                        NotSubscribed.Subscribed();
                        EXPECT_EQ(
                            helper::PubSubState::Subscribed,
                            StateMachine.GetState());

                        Subscribed.Unsubscribed();
                        NotSubscribed.Stopped();
                        EXPECT_EQ(
                            helper::PubSubState::ServiceDown,
                            StateMachine.GetState());
                    }

                    TEST_F(SubscribedStateTest, UnregisteredStateTransition)
                    {
                        // Without a state machine the transition request is a no-op.
                        SubscribedState _state;
                        _state.Activate(helper::PubSubState::NotSubscribed);

                        EXPECT_NO_THROW(_state.Unsubscribed());
                        EXPECT_NO_THROW(_state.Stopped());
                        EXPECT_EQ(helper::PubSubState::Subscribed, _state.GetState());
                    }
                }
            }
        }
    }
}
