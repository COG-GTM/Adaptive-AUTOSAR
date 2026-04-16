#include <cstdlib>
#include <termios.h>
#include <unistd.h>
#include <iostream>
#include "./argument_configuration.h"

namespace application
{
    namespace helper
    {
        const std::string ArgumentConfiguration::cConfigArgument{"config"};
        const std::string ArgumentConfiguration::cEvConfigArgument{"evconfig"};
        const std::string ArgumentConfiguration::cDmConfigArgument{"dmconfig"};
        const std::string ArgumentConfiguration::cPhmConfigArgument{"phmconfig"};
        const std::string ArgumentConfiguration::cApiKeyArgument{"vccapikey"};
        const std::string ArgumentConfiguration::cBearerTokenArgument{"bearertoken"};
        const std::string ArgumentConfiguration::cApiKeyEnvVar{"VCC_API_KEY"};
        const std::string ArgumentConfiguration::cBearerTokenEnvVar{"BEARER_TOKEN"};

        ArgumentConfiguration::ArgumentConfiguration(
            int argc,
            char *argv[],
            std::string defaultConfigFile,
            std::string extendedVehicleConfigFile,
            std::string diagnosticManagerConfigFile,
            std::string healthMonitoringConfigFile)
        {
            const int cConfigArgumentIndex{1};
            const int cEvConfigArgumentIndex{2};
            const int cDmConfigArgumentIndex{3};
            const int cPhmConfigArgumentIndex{4};

            if (argc > cPhmConfigArgumentIndex)
            {
                std::string _configFilepath{argv[cConfigArgumentIndex]};
                mArguments[cConfigArgument] = _configFilepath;

                std::string _evConfigFilepath{argv[cEvConfigArgumentIndex]};
                mArguments[cEvConfigArgument] = _evConfigFilepath;

                std::string _dmConfigFilepath{argv[cDmConfigArgumentIndex]};
                mArguments[cDmConfigArgument] = _dmConfigFilepath;

                std::string _phmConfigFilepath{argv[cPhmConfigArgumentIndex]};
                mArguments[cPhmConfigArgument] = _phmConfigFilepath;
            }
            else
            {
                mArguments[cConfigArgument] = defaultConfigFile;
                mArguments[cEvConfigArgument] = extendedVehicleConfigFile;
                mArguments[cDmConfigArgument] = diagnosticManagerConfigFile;
                mArguments[cPhmConfigArgument] = healthMonitoringConfigFile;
            }
        }

        bool ArgumentConfiguration::trySetEchoMode(bool enabled)
        {
            const int cSuccessfulCode{0};

            struct termios _tty;
            // Read the console input file descriptor attributes
            int _successful{tcgetattr(STDIN_FILENO, &_tty)};

            if (_successful == cSuccessfulCode)
            {
                if (enabled)
                {
                    // Enable the echo attribute flag
                    _tty.c_lflag |= ECHO;
                }
                else
                {
                    // Disable the echo attribute flag
                    _tty.c_lflag &= ~ECHO;
                }

                // Appy the modified attributes to the file descriptor immediately
                _successful = tcsetattr(STDIN_FILENO, TCSANOW, &_tty);
            }

            bool _result{_successful == cSuccessfulCode};
            return _result;
        }

        bool ArgumentConfiguration::tryAskSafely(
            std::string message, std::string argumentKey)
        {
            std::cout << message << std::endl;
            bool _result{trySetEchoMode(false)};

            if (_result)
            {
                std::string userInput;
                std::cin >> userInput;
                mArguments[argumentKey] = userInput;

                _result = trySetEchoMode(true);
                if (!_result)
                {
                    // Revert the arguments dictionary to the state before the function call
                    mArguments.erase(argumentKey);
                }
            }

            return _result;
        }

        bool ArgumentConfiguration::tryLoadFromEnv(
            std::string envVarName, std::string argumentKey)
        {
            const char *_envValue = std::getenv(envVarName.c_str());
            if (_envValue != nullptr && std::string(_envValue).length() > 0)
            {
                mArguments[argumentKey] = std::string(_envValue);
                return true;
            }
            return false;
        }

        const std::map<std::string, std::string> &ArgumentConfiguration::GetArguments() const noexcept
        {
            return mArguments;
        }

        bool ArgumentConfiguration::TryAskingVccApiKey(std::string message)
        {
            if (tryLoadFromEnv(cApiKeyEnvVar, cApiKeyArgument))
            {
                std::cout << "VCC API key loaded from environment variable."
                          << std::endl;
                return true;
            }
            return tryAskSafely(message, cApiKeyArgument);
        }

        bool ArgumentConfiguration::TryAskingBearToken(std::string message)
        {
            if (tryLoadFromEnv(cBearerTokenEnvVar, cBearerTokenArgument))
            {
                std::cout << "OAuth 2.0 bearer token loaded from environment variable."
                          << std::endl;
                return true;
            }
            return tryAskSafely(message, cBearerTokenArgument);
        }
    }
}
