#include <cstdlib>
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

static bool hasEnvSecrets()
{
    const char *apiKey = std::getenv("VCC_API_KEY");
    const char *bearerToken = std::getenv("BEARER_TOKEN");
    return (apiKey != nullptr && apiKey[0] != '\0') &&
           (bearerToken != nullptr && bearerToken[0] != '\0');
}

int main(int argc, char *argv[])
{
    application::helper::ArgumentConfiguration _argumentConfiguration(argc, argv);
    bool _nonInteractive{hasEnvSecrets()};

    bool _successful{_argumentConfiguration.TryAskingVccApiKey()};
    if (!_successful)
    {
        std::cout << "Asking for the VCC API key is faile!";
        return -1;
    }

    if (!_nonInteractive)
    {
        std::system("clear");
    }
    _successful = _argumentConfiguration.TryAskingBearToken();
    if (!_successful)
    {
        std::cout << "Asking for the OAuth 2.0 bear key is failed!";
        return -1;
    }

    running = true;
    executionManagement = new application::platform::ExecutionManagement(&poller);
    executionManagement->Initialize(_argumentConfiguration.GetArguments());

    std::future<void> _future{std::async(std::launch::async, performPolling)};

    std::getchar();
    if (!_nonInteractive)
    {
        std::system("clear");
    }
    std::getchar();

    int _result{executionManagement->Terminate()};
    running = false;
    _future.get();
    delete executionManagement;

    return _result;
}
