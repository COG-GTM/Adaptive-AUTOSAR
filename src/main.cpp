#include <csignal>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include "./application/helper/argument_configuration.h"
#include "./application/platform/execution_management.h"

bool running;
AsyncBsdSocketLib::Poller poller;
application::platform::ExecutionManagement *executionManagement;

void performPolling()
{
    const std::chrono::milliseconds cSleepDuration{
        ara::exec::DeterministicClient::cCycleDelayMs};

    while (running)
    {
        poller.TryPoll();
        std::this_thread::sleep_for(cSleepDuration);
    }
}

/// @brief Block the termination signals so they can be consumed synchronously
/// @param[out] signalSet Blocked signal set
/// @note Must be called before spawning any thread so the mask is inherited
static void blockTerminationSignals(sigset_t &signalSet)
{
    sigemptyset(&signalSet);
    sigaddset(&signalSet, SIGINT);
    sigaddset(&signalSet, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &signalSet, nullptr);
}

/// @brief Wait until SIGINT/SIGTERM arrives or the optional run duration elapses
static void waitForShutdown(
    const sigset_t &signalSet,
    const application::helper::ArgumentConfiguration &configuration)
{
    std::chrono::milliseconds _runDuration;
    if (!configuration.TryGetRunDuration(_runDuration))
    {
        int _signal;
        sigwait(&signalSet, &_signal);
        return;
    }

    const auto cDeadline{std::chrono::steady_clock::now() + _runDuration};

    while (true)
    {
        const auto _now{std::chrono::steady_clock::now()};
        if (_now >= cDeadline)
        {
            return;
        }

        const auto _remaining{
            std::chrono::duration_cast<std::chrono::nanoseconds>(cDeadline - _now)};
        struct timespec _timeout;
        _timeout.tv_sec = static_cast<time_t>(_remaining.count() / 1000000000LL);
        _timeout.tv_nsec = static_cast<long>(_remaining.count() % 1000000000LL);

        siginfo_t _info;
        if (sigtimedwait(&signalSet, &_info, &_timeout) > 0)
        {
            return;
        }
        // EAGAIN (timeout) or EINTR: re-evaluate the deadline
    }
}

static void waitForConsole()
{
    std::getchar();
    std::system("clear");
    std::getchar();
}

int main(int argc, char *argv[])
{
    application::helper::ArgumentConfiguration _argumentConfiguration(argc, argv);
    const bool cNonInteractive{_argumentConfiguration.IsNonInteractive()};

    sigset_t _signalSet;
    if (cNonInteractive)
    {
        blockTerminationSignals(_signalSet);
    }

    bool _successful{_argumentConfiguration.TryAskingVccApiKey()};
    if (!_successful)
    {
        std::cerr << "Failed to obtain the VCC API key." << std::endl;
        return -1;
    }

    if (!cNonInteractive)
    {
        std::system("clear");
    }
    _successful = _argumentConfiguration.TryAskingBearToken();
    if (!_successful)
    {
        std::cerr << "Failed to obtain the OAuth 2.0 bearer token." << std::endl;
        return -1;
    }

    running = true;
    executionManagement = new application::platform::ExecutionManagement(&poller);
    executionManagement->Initialize(_argumentConfiguration.GetArguments());

    std::future<void> _future{std::async(std::launch::async, performPolling)};

    if (cNonInteractive)
    {
        waitForShutdown(_signalSet, _argumentConfiguration);
    }
    else
    {
        waitForConsole();
    }

    int _result{executionManagement->Terminate()};
    running = false;
    _future.get();
    delete executionManagement;

    return _result;
}
