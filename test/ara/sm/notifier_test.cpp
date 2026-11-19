#include <functional>
#include <vector>
#include <gtest/gtest.h>
#include "../../../src/ara/sm/states.h"
#include "../../../src/ara/sm/notifier.h"

namespace ara
{
    namespace sm
    {
        TEST(SMNotifierTest, ReadReturnsInitialState)
        {
            FunctionGroupStates _state = FunctionGroupStates::kVerify;
            const Notifier<FunctionGroupStates> cNotifier(_state);

            EXPECT_EQ(FunctionGroupStates::kVerify, cNotifier.Read());
        }

        TEST(SMNotifierTest, ReadTracksTheReferencedState)
        {
            FunctionGroupStates _state = FunctionGroupStates::kOff;
            Notifier<FunctionGroupStates> _notifier(_state);

            _state = FunctionGroupStates::kUpdate;

            EXPECT_EQ(FunctionGroupStates::kUpdate, _notifier.Read());
        }

        TEST(SMNotifierTest, NotifyWithoutSubscribersIsNoOp)
        {
            FunctionGroupStates _state = FunctionGroupStates::kOff;
            Notifier<FunctionGroupStates> _notifier(_state);

            EXPECT_NO_THROW(_notifier.Notify());
            EXPECT_EQ(FunctionGroupStates::kOff, _state);
        }

        TEST(SMNotifierTest, SubscribeDoesNotInvokeHandler)
        {
            FunctionGroupStates _state = FunctionGroupStates::kOff;
            Notifier<FunctionGroupStates> _notifier(_state);
            int _counter = 0;
            NotificationHandler<FunctionGroupStates> _handler =
                [&_counter](FunctionGroupStates)
            { ++_counter; };

            _notifier.Subscribe(_handler);

            EXPECT_EQ(0, _counter);
        }

        TEST(SMNotifierTest, NotifyPassesTheCurrentStateOnEachCall)
        {
            FunctionGroupStates _state = FunctionGroupStates::kOff;
            Notifier<FunctionGroupStates> _notifier(_state);
            std::vector<FunctionGroupStates> _receivedStates;
            NotificationHandler<FunctionGroupStates> _handler =
                [&_receivedStates](FunctionGroupStates newState)
            { _receivedStates.push_back(newState); };
            _notifier.Subscribe(_handler);

            _notifier.Notify();
            _state = FunctionGroupStates::kRunning;
            _notifier.Notify();
            _notifier.Notify();

            const std::vector<FunctionGroupStates> cExpectedStates{
                FunctionGroupStates::kOff,
                FunctionGroupStates::kRunning,
                FunctionGroupStates::kRunning};
            EXPECT_EQ(cExpectedStates, _receivedStates);
        }

        TEST(SMNotifierTest, AllSubscribersAreNotifiedInSubscriptionOrder)
        {
            FunctionGroupStates _state = FunctionGroupStates::kRunning;
            Notifier<FunctionGroupStates> _notifier(_state);
            std::vector<int> _invocationOrder;
            for (int i = 0; i < 3; ++i)
            {
                NotificationHandler<FunctionGroupStates> _handler =
                    [&_invocationOrder, i](FunctionGroupStates newState)
                {
                    EXPECT_EQ(FunctionGroupStates::kRunning, newState);
                    _invocationOrder.push_back(i);
                };
                _notifier.Subscribe(_handler);
            }

            _notifier.Notify();

            const std::vector<int> cExpectedOrder{0, 1, 2};
            EXPECT_EQ(cExpectedOrder, _invocationOrder);
        }

        TEST(SMNotifierTest, SameHandlerSubscribedTwiceIsInvokedTwice)
        {
            FunctionGroupStates _state = FunctionGroupStates::kOff;
            Notifier<FunctionGroupStates> _notifier(_state);
            int _counter = 0;
            NotificationHandler<FunctionGroupStates> _handler =
                [&_counter](FunctionGroupStates)
            { ++_counter; };
            _notifier.Subscribe(_handler);
            _notifier.Subscribe(_handler);

            _notifier.Notify();

            EXPECT_EQ(2, _counter);
        }

        TEST(SMNotifierTest, LateSubscriberOnlySeesLaterNotifications)
        {
            FunctionGroupStates _state = FunctionGroupStates::kOff;
            Notifier<FunctionGroupStates> _notifier(_state);
            int _earlyCounter = 0;
            int _lateCounter = 0;
            NotificationHandler<FunctionGroupStates> _earlyHandler =
                [&_earlyCounter](FunctionGroupStates)
            { ++_earlyCounter; };
            NotificationHandler<FunctionGroupStates> _lateHandler =
                [&_lateCounter](FunctionGroupStates)
            { ++_lateCounter; };

            _notifier.Subscribe(_earlyHandler);
            _notifier.Notify();
            _notifier.Subscribe(_lateHandler);
            _notifier.Notify();

            EXPECT_EQ(2, _earlyCounter);
            EXPECT_EQ(1, _lateCounter);
        }

        TEST(SMNotifierTest, HandlerCannotChangeTheStateThroughItsArgument)
        {
            FunctionGroupStates _state = FunctionGroupStates::kOff;
            Notifier<FunctionGroupStates> _notifier(_state);
            NotificationHandler<FunctionGroupStates> _handler =
                [](FunctionGroupStates newState)
            { newState = FunctionGroupStates::kVerify; (void)newState; };
            _notifier.Subscribe(_handler);

            _notifier.Notify();

            EXPECT_EQ(FunctionGroupStates::kOff, _notifier.Read());
        }

        /// @note Characterisation: Subscribe() accepts an empty handler, and the
        ///       resulting std::bad_function_call stops later subscribers from
        ///       being notified.
        TEST(SMNotifierTest, EmptyHandlerThrowsAndSkipsRemainingSubscribers)
        {
            FunctionGroupStates _state = FunctionGroupStates::kOff;
            Notifier<FunctionGroupStates> _notifier(_state);
            int _counter = 0;
            NotificationHandler<FunctionGroupStates> _emptyHandler;
            NotificationHandler<FunctionGroupStates> _handler =
                [&_counter](FunctionGroupStates)
            { ++_counter; };
            _notifier.Subscribe(_emptyHandler);
            _notifier.Subscribe(_handler);

            EXPECT_THROW(_notifier.Notify(), std::bad_function_call);
            EXPECT_EQ(0, _counter);
        }

        /// @note BUG (documented, not fixed): Notify() iterates mSubsribers with a
        ///       range-based loop, so a handler that calls Subscribe() during the
        ///       notification can reallocate the vector and invalidate the loop
        ///       iterators (undefined behaviour; heap-use-after-free under ASan).
        ///       Disabled because it exercises undefined behaviour.
        TEST(SMNotifierTest, DISABLED_SubscribeDuringNotifyInvalidatesIteration)
        {
            FunctionGroupStates _state = FunctionGroupStates::kOff;
            Notifier<FunctionGroupStates> _notifier(_state);
            int _lateCounter = 0;
            int _tailCounter = 0;
            NotificationHandler<FunctionGroupStates> _lateHandler =
                [&_lateCounter](FunctionGroupStates)
            { ++_lateCounter; };
            NotificationHandler<FunctionGroupStates> _subscribingHandler =
                [&_notifier, &_lateHandler](FunctionGroupStates)
            {
                for (int i = 0; i < 64; ++i)
                {
                    _notifier.Subscribe(_lateHandler);
                }
            };
            NotificationHandler<FunctionGroupStates> _tailHandler =
                [&_tailCounter](FunctionGroupStates)
            { ++_tailCounter; };
            _notifier.Subscribe(_subscribingHandler);
            _notifier.Subscribe(_tailHandler);

            _notifier.Notify();

            // Expected once fixed: the handlers subscribed before Notify() are
            // each invoked exactly once and the iteration stays well defined.
            EXPECT_EQ(1, _tailCounter);
            EXPECT_EQ(0, _lateCounter);
        }
    }
}
