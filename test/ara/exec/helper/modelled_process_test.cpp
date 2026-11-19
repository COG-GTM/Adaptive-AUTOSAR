#include <thread>
#include <gtest/gtest.h>
#include "../../../../src/ara/exec/helper/modelled_process.h"

namespace ara
{
    namespace exec
    {
        namespace helper
        {
            class ModelledProcessTest : public ModelledProcess, public testing::Test
            {
            private:
                AsyncBsdSocketLib::Poller mPoller;

            protected:
                const int cExitCode{0};

                int Main(
                    const std::atomic_bool *cancellationToken,
                    const std::map<std::string, std::string> &arguments) override
                {
                    const std::chrono::milliseconds cSleepTime{1};

                    while (!cancellationToken->load())
                    {
                        std::this_thread::sleep_for(cSleepTime);
                    }

                    return cExitCode;
                }

            public:
                ModelledProcessTest() : ModelledProcess("TestApp", &mPoller)
                {
                }
            };

            TEST_F(ModelledProcessTest, InitializeMethod)
            {
                std::map<std::string, std::string> _arguments;
                Initialize(_arguments);
                int _actualResult{Terminate()};

                EXPECT_EQ(cExitCode, _actualResult);
            }

            TEST_F(ModelledProcessTest, TerminateMethod)
            {
                const int cExpectedResult{0};
                int _actualResult{Terminate()};

                EXPECT_EQ(cExpectedResult, _actualResult);
            }

            TEST_F(ModelledProcessTest, RedundantTerminateScenario)
            {
                std::map<std::string, std::string> _arguments;
                Initialize(_arguments);

                EXPECT_EQ(cExitCode, Terminate());
                // The process has been already disposed
                EXPECT_EQ(cSuccessfulExitCode, Terminate());
            }

            TEST_F(ModelledProcessTest, WaitForActivationMethod)
            {
                EXPECT_TRUE(WaitForActivation());
            }

            TEST_F(ModelledProcessTest, LogMethod)
            {
                log::LogStream _logStream;
                _logStream << "Modelled process unit test log";

                EXPECT_NO_THROW(Log(cLogLevel, _logStream));
                EXPECT_NO_THROW(Log(cErrorLevel, _logStream));
            }

            class ModelledProcessProbeTest : public ModelledProcess, public testing::Test
            {
            private:
                AsyncBsdSocketLib::Poller mPoller;

            protected:
                const int cExitCode{3};

                std::atomic_int RunCount;
                std::atomic_bool CancelledAtEntry;
                std::map<std::string, std::string> ReceivedArguments;

                int Main(
                    const std::atomic_bool *cancellationToken,
                    const std::map<std::string, std::string> &arguments) override
                {
                    const std::chrono::milliseconds cSleepTime{1};

                    CancelledAtEntry = cancellationToken->load();
                    ReceivedArguments = arguments;
                    ++RunCount;

                    while (!cancellationToken->load())
                    {
                        std::this_thread::sleep_for(cSleepTime);
                    }

                    return cExitCode;
                }

                void WaitForRunCount(int runCount)
                {
                    const std::chrono::milliseconds cSleepTime{1};
                    const int cMaxRetries{5000};

                    for (int i = 0; i < cMaxRetries && RunCount < runCount; ++i)
                    {
                        std::this_thread::sleep_for(cSleepTime);
                    }
                }

                void TearDown() override
                {
                    // Join the main block before the fixture members are disposed
                    Terminate();
                }

            public:
                ModelledProcessProbeTest() : ModelledProcess("ProbeApp", &mPoller),
                                             RunCount{0},
                                             CancelledAtEntry{false}
                {
                }
            };

            TEST_F(ModelledProcessProbeTest, ExitCodePropagation)
            {
                const std::map<std::string, std::string> cArguments{{"key", "value"}};
                const int cExpectedRunCount{1};
                Initialize(cArguments);
                WaitForRunCount(cExpectedRunCount);
                int _actualResult{Terminate()};

                EXPECT_EQ(cExitCode, _actualResult);
                EXPECT_EQ(cArguments, ReceivedArguments);
                EXPECT_FALSE(CancelledAtEntry);
            }

            TEST_F(ModelledProcessProbeTest, RedundantInitializeScenario)
            {
                const int cExpectedRunCount{1};
                std::map<std::string, std::string> _arguments;
                Initialize(_arguments);
                Initialize(_arguments);
                WaitForRunCount(cExpectedRunCount);
                int _actualResult{Terminate()};

                EXPECT_EQ(cExitCode, _actualResult);
                EXPECT_EQ(cExpectedRunCount, RunCount);
            }

            // Documents a suspected production bug (not fixed here on purpose):
            // ModelledProcess::Terminate sets the cancellation token, but nothing
            // resets it, so a process that is initialized again after termination
            // finds its main block already cancelled at entry.
            // Enable the test once ModelledProcess::Initialize resets the token.
            TEST_F(ModelledProcessProbeTest, DISABLED_BugReinitializeAfterTerminateStartsCancelled)
            {
                const int cFirstRunCount{1};
                const int cExpectedRunCount{2};
                std::map<std::string, std::string> _arguments;

                Initialize(_arguments);
                WaitForRunCount(cFirstRunCount);
                Terminate();
                Initialize(_arguments);
                // Nothing cancels the second run before its main block is entered
                WaitForRunCount(cExpectedRunCount);

                EXPECT_EQ(cExpectedRunCount, RunCount);
                EXPECT_FALSE(CancelledAtEntry);
            }
        }
    }
}