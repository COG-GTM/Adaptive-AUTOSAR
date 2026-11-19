#include <functional>
#include <vector>
#include <gtest/gtest.h>
#include "../../../src/ara/sm/states.h"
#include "../../../src/ara/sm/trigger_in.h"
#include "../../../src/ara/sm/trigger_out.h"
#include "../../../src/ara/sm/trigger_inout.h"

namespace ara
{
    namespace sm
    {
        TEST(SMTriggerWrappersTest, TriggerInInvokesHandlerOnWrite)
        {
            FunctionGroupStates _state = FunctionGroupStates::kOff;
            int _counter = 0;
            TriggerHandler _handler = [&_counter]()
            { ++_counter; };
            TriggerIn<FunctionGroupStates> _triggerIn(_state, _handler);

            EXPECT_EQ(0, _counter);
            _triggerIn.GetTrigger().Write(FunctionGroupStates::kUpdate);

            EXPECT_EQ(1, _counter);
            EXPECT_EQ(FunctionGroupStates::kUpdate, _state);
        }

        TEST(SMTriggerWrappersTest, TriggerInReturnsTheSameTrigger)
        {
            FunctionGroupStates _state = FunctionGroupStates::kOff;
            TriggerHandler _handler = []() {};
            TriggerIn<FunctionGroupStates> _triggerIn(_state, _handler);

            Trigger<FunctionGroupStates> &_first = _triggerIn.GetTrigger();
            Trigger<FunctionGroupStates> &_second = _triggerIn.GetTrigger();

            EXPECT_EQ(&_first, &_second);
        }

        TEST(SMTriggerWrappersTest, TriggerOutReadTracksState)
        {
            FunctionGroupStates _state = FunctionGroupStates::kOff;
            TriggerOut<FunctionGroupStates> _triggerOut(_state);

            EXPECT_EQ(FunctionGroupStates::kOff, _triggerOut.GetNotifier().Read());
            _state = FunctionGroupStates::kRunning;
            EXPECT_EQ(
                FunctionGroupStates::kRunning, _triggerOut.GetNotifier().Read());
        }

        TEST(SMTriggerWrappersTest, TriggerOutNotifiesSubscriberWithState)
        {
            FunctionGroupStates _state = FunctionGroupStates::kOff;
            TriggerOut<FunctionGroupStates> _triggerOut(_state);
            std::vector<FunctionGroupStates> _receivedStates;
            NotificationHandler<FunctionGroupStates> _handler =
                [&_receivedStates](FunctionGroupStates newState)
            { _receivedStates.push_back(newState); };
            _triggerOut.GetNotifier().Subscribe(_handler);

            _state = FunctionGroupStates::kVerify;
            _triggerOut.GetNotifier().Notify();

            const std::vector<FunctionGroupStates> cExpectedStates{
                FunctionGroupStates::kVerify};
            EXPECT_EQ(cExpectedStates, _receivedStates);
        }

        TEST(SMTriggerWrappersTest, TriggerInOutNotifierReadsWhatTriggerWrote)
        {
            FunctionGroupStates _state = FunctionGroupStates::kOff;
            TriggerHandler _handler = []() {};
            TriggerInOut<FunctionGroupStates> _triggerInOut(_state, _handler);

            _triggerInOut.GetTrigger().Write(FunctionGroupStates::kRunning);

            EXPECT_EQ(
                FunctionGroupStates::kRunning,
                _triggerInOut.GetNotifier().Read());
            EXPECT_EQ(FunctionGroupStates::kRunning, _state);
        }

        TEST(SMTriggerWrappersTest, TriggerInOutWriteDoesNotNotifyImplicitly)
        {
            FunctionGroupStates _state = FunctionGroupStates::kOff;
            TriggerHandler _triggerHandler = []() {};
            TriggerInOut<FunctionGroupStates> _triggerInOut(
                _state, _triggerHandler);
            int _counter = 0;
            NotificationHandler<FunctionGroupStates> _handler =
                [&_counter](FunctionGroupStates)
            { ++_counter; };
            _triggerInOut.GetNotifier().Subscribe(_handler);

            _triggerInOut.GetTrigger().Write(FunctionGroupStates::kRunning);
            EXPECT_EQ(0, _counter);

            _triggerInOut.GetNotifier().Notify();
            EXPECT_EQ(1, _counter);
        }

        TEST(SMTriggerWrappersTest, TriggerInOutChainDeliversEveryWrittenState)
        {
            FunctionGroupStates _state = FunctionGroupStates::kOff;
            TriggerInOut<FunctionGroupStates> *_triggerInOutPtr = nullptr;
            TriggerHandler _triggerHandler = [&_triggerInOutPtr]()
            { _triggerInOutPtr->GetNotifier().Notify(); };
            TriggerInOut<FunctionGroupStates> _triggerInOut(
                _state, _triggerHandler);
            _triggerInOutPtr = &_triggerInOut;
            std::vector<FunctionGroupStates> _receivedStates;
            NotificationHandler<FunctionGroupStates> _handler =
                [&_receivedStates](FunctionGroupStates newState)
            { _receivedStates.push_back(newState); };
            _triggerInOut.GetNotifier().Subscribe(_handler);

            _triggerInOut.GetTrigger().Write(FunctionGroupStates::kRunning);
            _triggerInOut.GetTrigger().Write(FunctionGroupStates::kUpdate);
            _triggerInOut.GetTrigger().Write(FunctionGroupStates::kVerify);
            _triggerInOut.GetTrigger().Write(FunctionGroupStates::kOff);

            const std::vector<FunctionGroupStates> cExpectedStates{
                FunctionGroupStates::kRunning,
                FunctionGroupStates::kUpdate,
                FunctionGroupStates::kVerify,
                FunctionGroupStates::kOff};
            EXPECT_EQ(cExpectedStates, _receivedStates);
        }
    }
}
