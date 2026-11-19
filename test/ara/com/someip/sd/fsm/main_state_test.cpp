#include <gtest/gtest.h>
#include <thread>
#include "../../../../../../src/ara/com/someip/sd/fsm/main_state.h"

namespace ara
{
    namespace com
    {
        namespace someip
        {
            namespace sd
            {
                namespace fsm
                {
                    TEST(MainStateTest, NegativeCyclicOfferDelay)
                    {
                        const int cCyclicOfferDelay{-1};
                        const std::function<void()> cOnTimerExpired{
                            []()
                            {
                                // Empty callback
                            }};

                        EXPECT_THROW(
                            MainState _machineState(cOnTimerExpired, cCyclicOfferDelay),
                            std::invalid_argument);
                    }

                    TEST(MainStateTest, CyclicTimerUntilServiceStopped)
                    {
                        const int cCyclicOfferDelay{1};
                        const uint32_t cExpectedCycles{3};
                        uint32_t _counter{0};
                        MainState *_machineStatePtr{nullptr};
                        const std::function<void()> cOnTimerExpired{
                            [&]()
                            {
                                ++_counter;
                                if (_counter == cExpectedCycles)
                                {
                                    _machineStatePtr->ServiceStopped();
                                }
                            }};

                        MainState _machineState(cOnTimerExpired, cCyclicOfferDelay);
                        _machineStatePtr = &_machineState;

                        // The activation blocks until the service is stopped by the callback.
                        _machineState.Activate(helper::SdServerState::RepetitionPhase);

                        EXPECT_EQ(cExpectedCycles, _counter);
                        EXPECT_EQ(helper::SdServerState::MainPhase, _machineState.GetState());
                    }

                    TEST(MainStateTest, NoTimerAfterServiceStopped)
                    {
                        const int cCyclicOfferDelay{1};
                        uint32_t _counter{0};
                        const std::function<void()> cOnTimerExpired{
                            [&]()
                            {
                                ++_counter;
                            }};

                        MainState _machineState(cOnTimerExpired, cCyclicOfferDelay);
                        _machineState.ServiceStopped();
                        _machineState.Activate(helper::SdServerState::RepetitionPhase);

                        EXPECT_EQ(0U, _counter);
                    }
                }
            }
        }
    }
}
