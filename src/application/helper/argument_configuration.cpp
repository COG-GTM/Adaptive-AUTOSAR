#include <cstdlib>
#include <termios.h>
#include <unistd.h>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>
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
        const std::string ArgumentConfiguration::cNonInteractiveArgument{"noninteractive"};
        const std::string ArgumentConfiguration::cRunDurationArgument{"runduration"};
        const std::string ArgumentConfiguration::cApiKeyEnvVar{"VCC_API_KEY"};
        const std::string ArgumentConfiguration::cBearerTokenEnvVar{"BEARER_TOKEN"};
        const std::string ArgumentConfiguration::cRunDurationEnvVar{"RUN_DURATION_MS"};
        const std::string ArgumentConfiguration::cEnvVarPrefix{"ADAPTIVE_AUTOSAR_"};
        const std::string ArgumentConfiguration::cOptionPrefix{"--"};

        ArgumentConfiguration::ArgumentConfiguration(
            int argc,
            char *argv[],
            std::string defaultConfigFile,
            std::string extendedVehicleConfigFile,
            std::string diagnosticManagerConfigFile,
            std::string healthMonitoringConfigFile)
        {
            parseCommandLine(argc, argv);

            applyDefault(cConfigArgument, defaultConfigFile);
            applyDefault(cEvConfigArgument, extendedVehicleConfigFile);
            applyDefault(cDmConfigArgument, diagnosticManagerConfigFile);
            applyDefault(cPhmConfigArgument, healthMonitoringConfigFile);

            if (mArguments.find(cRunDurationArgument) == mArguments.end())
            {
                tryLoadFromEnv(cRunDurationEnvVar, cRunDurationArgument);
            }

            if (mArguments.find(cNonInteractiveArgument) == mArguments.end())
            {
                tryLoadFromEnv(
                    GetEnvVarName(cNonInteractiveArgument), cNonInteractiveArgument);
            }
        }

        std::string ArgumentConfiguration::GetEnvVarName(
            const std::string &argumentKey)
        {
            std::string _result{cEnvVarPrefix};
            std::transform(
                argumentKey.begin(), argumentKey.end(),
                std::back_inserter(_result),
                [](unsigned char c)
                { return static_cast<char>(std::toupper(c)); });

            return _result;
        }

        void ArgumentConfiguration::parseCommandLine(int argc, char *argv[])
        {
            const std::vector<std::string> cPositionalKeys{
                cConfigArgument,
                cEvConfigArgument,
                cDmConfigArgument,
                cPhmConfigArgument};
            const char cAssignment{'='};
            const std::string cFlagValue{"true"};

            std::size_t _positionalIndex{0};

            for (int i = 1; i < argc; ++i)
            {
                std::string _argument{argv[i]};

                if (_argument.compare(0, cOptionPrefix.size(), cOptionPrefix) == 0)
                {
                    std::string _option{_argument.substr(cOptionPrefix.size())};
                    std::size_t _assignmentPosition{_option.find(cAssignment)};

                    if (_assignmentPosition == std::string::npos)
                    {
                        mArguments[_option] = cFlagValue;
                    }
                    else
                    {
                        std::string _key{_option.substr(0, _assignmentPosition)};
                        std::string _value{_option.substr(_assignmentPosition + 1)};
                        mArguments[_key] = _value;
                    }
                }
                else if (_positionalIndex < cPositionalKeys.size())
                {
                    // Explicit '--key=value' options take precedence over positional arguments.
                    const std::string &_key{cPositionalKeys[_positionalIndex]};
                    if (mArguments.find(_key) == mArguments.end())
                    {
                        mArguments[_key] = _argument;
                    }

                    ++_positionalIndex;
                }
            }
        }

        void ArgumentConfiguration::applyDefault(
            const std::string &argumentKey, const std::string &defaultValue)
        {
            if (mArguments.find(argumentKey) != mArguments.end())
            {
                return;
            }

            if (!tryLoadFromEnv(GetEnvVarName(argumentKey), argumentKey))
            {
                mArguments[argumentKey] = defaultValue;
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
            if (_envValue != nullptr && _envValue[0] != '\0')
            {
                mArguments[argumentKey] = std::string(_envValue);
                return true;
            }
            return false;
        }

        bool ArgumentConfiguration::tryLoadSecret(
            const std::string &envVarName,
            const std::string &argumentKey,
            const std::string &description,
            const std::string &message)
        {
            if (tryLoadFromEnv(envVarName, argumentKey))
            {
                if (!IsNonInteractive())
                {
                    std::cout << description
                              << " loaded from environment variable."
                              << std::endl;
                }
                return true;
            }

            if (IsNonInteractive())
            {
                std::cerr << description << " is not set. Provide it via the '"
                          << envVarName << "' environment variable."
                          << std::endl;
                return false;
            }

            return tryAskSafely(message, argumentKey);
        }

        const std::map<std::string, std::string> &ArgumentConfiguration::GetArguments() const noexcept
        {
            return mArguments;
        }

        bool ArgumentConfiguration::IsNonInteractive() const
        {
            const std::string cDisabledValue{"0"};
            const std::string cDisabledWord{"false"};

            auto _iterator{mArguments.find(cNonInteractiveArgument)};
            if (_iterator != mArguments.end())
            {
                const std::string &_value{_iterator->second};
                return _value != cDisabledValue && _value != cDisabledWord;
            }

            return isatty(STDIN_FILENO) == 0;
        }

        bool ArgumentConfiguration::TryGetRunDuration(
            std::chrono::milliseconds &duration) const
        {
            auto _iterator{mArguments.find(cRunDurationArgument)};
            if (_iterator == mArguments.end())
            {
                return false;
            }

            try
            {
                std::size_t _consumed{0};
                long long _milliseconds{std::stoll(_iterator->second, &_consumed)};

                if (_consumed != _iterator->second.size() || _milliseconds <= 0)
                {
                    return false;
                }

                duration = std::chrono::milliseconds{_milliseconds};
                return true;
            }
            catch (const std::logic_error &)
            {
                return false;
            }
        }

        bool ArgumentConfiguration::TryAskingVccApiKey(std::string message)
        {
            return tryLoadSecret(
                cApiKeyEnvVar, cApiKeyArgument, "VCC API key", message);
        }

        bool ArgumentConfiguration::TryAskingBearToken(std::string message)
        {
            return tryLoadSecret(
                cBearerTokenEnvVar,
                cBearerTokenArgument,
                "OAuth 2.0 bearer token",
                message);
        }
    }
}
