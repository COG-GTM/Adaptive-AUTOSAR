#include <functional>
#include <vector>
#include <gtest/gtest.h>
#include "../../../src/ara/sm/states.h"
#include "../../../src/ara/sm/trigger.h"

namespace ara
{
    namespace sm
    {
        TEST(SMTriggerTest, ConstructionDoesNotInvokeHandlerOrChangeState)
        {
            FunctionGroupStates _state = FunctionGroupStates::kOff;
            int _counter = 0;
            TriggerHandler _handler = [&_counter]()
            { ++_counter; };

            Trigger<FunctionGroupStates> _trigger(_state, _handler);

            EXPECT_EQ(0, _counter);
            EXPECT_EQ(FunctionGroupStates::kOff, _state);
        }

        TEST(SMTriggerTest, WriteInvokesHandlerExactlyOncePerWrite)
        {
            FunctionGroupStates _state = FunctionGroupStates::kOff;
            int _counter = 0;
            TriggerHandler _handler = [&_counter]()
            { ++_counter; };
            Trigger<FunctionGroupStates> _trigger(_state, _handler);

            _trigger.Write(FunctionGroupStates::kRunning);
            EXPECT_EQ(1, _counter);

            _trigger.Write(FunctionGroupStates::kUpdate);
            _trigger.Write(FunctionGroupStates::kVerify);
            EXPECT_EQ(3, _counter);
            EXPECT_EQ(FunctionGroupStates::kVerify, _state);
        }

        TEST(SMTriggerTest, HandlerObservesTheNewState)
        {
            FunctionGroupStates _state = FunctionGroupStates::kOff;
            std::vector<FunctionGroupStates> _observedStates;
            TriggerHandler _handler = [&_state, &_observedStates]()
            { _observedStates.push_back(_state); };
            Trigger<FunctionGroupStates> _trigger(_state, _handler);

            _trigger.Write(FunctionGroupStates::kRunning);
            _trigger.Write(FunctionGroupStates::kOff);

            const std::vector<FunctionGroupStates> cExpectedStates{
                FunctionGroupStates::kRunning, FunctionGroupStates::kOff};
            EXPECT_EQ(cExpectedStates, _observedStates);
        }

        TEST(SMTriggerTest, WritingTheSameStateStillInvokesHandler)
        {
            FunctionGroupStates _state = FunctionGroupStates::kRunning;
            int _counter = 0;
            TriggerHandler _handler = [&_counter]()
            { ++_counter; };
            Trigger<FunctionGroupStates> _trigger(_state, _handler);

            _trigger.Write(FunctionGroupStates::kRunning);

            EXPECT_EQ(1, _counter);
            EXPECT_EQ(FunctionGroupStates::kRunning, _state);
        }

        TEST(SMTriggerTest, WorksWithNonEnumStateType)
        {
            int _state = 0;
            int _counter = 0;
            TriggerHandler _handler = [&_counter]()
            { ++_counter; };
            Trigger<int> _trigger(_state, _handler);

            _trigger.Write(42);

            EXPECT_EQ(42, _state);
            EXPECT_EQ(1, _counter);
        }

        /// @note Characterisation: an empty handler is accepted by the constructor,
        ///       and Write() changes the state before std::function throws.
        TEST(SMTriggerTest, EmptyHandlerThrowsAfterStateIsWritten)
        {
            FunctionGroupStates _state = FunctionGroupStates::kOff;
            TriggerHandler _emptyHandler;
            Trigger<FunctionGroupStates> _trigger(_state, _emptyHandler);

            EXPECT_THROW(
                _trigger.Write(FunctionGroupStates::kRunning),
                std::bad_function_call);
            EXPECT_EQ(FunctionGroupStates::kRunning, _state);
        }
    }
}
