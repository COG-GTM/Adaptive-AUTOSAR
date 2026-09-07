#include <csignal>
#include <cstdlib>
#include "./application/helper/argument_configuration.h"
#include "./application/platform/execution_management.h"
#include "./ara/telemetry/telemetry_hub.h"
#include "./ara/telemetry/telemetry_server.h"

bool running;
AsyncBsdSocketLib::Poller poller;
application::platform::ExecutionManagement *executionManagement;

namespace
{
    const std::string cDashboardPortEnvVar{"DASHBOARD_PORT"};
    const std::string cDashboardRootEnvVar{"DASHBOARD_ROOT"};
    const uint16_t cDefaultDashboardPort{8088};
    const std::string cDefaultDashboardRoot{"./web"};

    std::atomic_bool interrupted{false};

    void onInterrupted(int)
    {
        interrupted = true;
    }

    std::string getEnvironmentVariable(
        const std::string &key, std::string defaultValue)
    {
        const char *cValue{std::getenv(key.c_str())};

        return (cValue != nullptr && cValue[0] != '\0')
                   ? std::string(cValue)
                   : std::move(defaultValue);
    }

    uint16_t getDashboardPort()
    {
        const std::string cPort{
            getEnvironmentVariable(
                cDashboardPortEnvVar,
                std::to_string(cDefaultDashboardPort))};

        try
        {
            return static_cast<uint16_t>(std::stoul(cPort));
        }
        catch (const std::exception &)
        {
            return cDefaultDashboardPort;
        }
    }
}

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
    const char *_apiKey = std::getenv(
        application::helper::ArgumentConfiguration::cApiKeyEnvVar.c_str());
    const char *_bearerToken = std::getenv(
        application::helper::ArgumentConfiguration::cBearerTokenEnvVar.c_str());
    return (_apiKey != nullptr && _apiKey[0] != '\0') &&
           (_bearerToken != nullptr && _bearerToken[0] != '\0');
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

    const uint16_t cDashboardPort{getDashboardPort()};
    ara::telemetry::TelemetryServer _telemetryServer(
        &ara::telemetry::TelemetryHub::Instance(),
        cDashboardPort,
        getEnvironmentVariable(cDashboardRootEnvVar, cDefaultDashboardRoot));

    if (_telemetryServer.Start())
    {
        std::cout << "ECU cockpit dashboard is served at http://127.0.0.1:"
                  << _telemetryServer.Port() << '\n';
    }
    else
    {
        std::cout << "Serving the ECU cockpit dashboard at port "
                  << cDashboardPort << " failed.\n";
    }

    std::signal(SIGINT, onInterrupted);
    std::signal(SIGTERM, onInterrupted);

    running = true;
    executionManagement = new application::platform::ExecutionManagement(&poller);
    executionManagement->Initialize(_argumentConfiguration.GetArguments());

    std::future<void> _future{std::async(std::launch::async, performPolling)};

    if (_nonInteractive)
    {
        // Keep the platform alive so that the dashboard can observe the runtime
        // until the process is interrupted.
        const std::chrono::milliseconds cSleepDuration{100};
        while (!interrupted)
        {
            std::this_thread::sleep_for(cSleepDuration);
        }
    }
    else
    {
        std::getchar();
        std::system("clear");
        std::getchar();
    }

    int _result{executionManagement->Terminate()};
    running = false;
    _future.get();
    _telemetryServer.Stop();
    delete executionManagement;

    return _result;
}
