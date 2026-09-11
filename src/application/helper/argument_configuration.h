#ifndef ARGUMENT_CONFIGURATION_H
#define ARGUMENT_CONFIGURATION_H

#include <chrono>
#include <map>
#include <string>

namespace application
{
    namespace helper
    {
        /// @brief A helper class to manage the arguments passed to the main application
        /// @details Each argument key is resolved in the following order:
        /// 1. Command line ('--key=value', '--key', or positional manifest paths)
        /// 2. Environment variable ('ADAPTIVE_AUTOSAR_<KEY>' or a dedicated variable)
        /// 3. Default value
        class ArgumentConfiguration
        {
        private:
            std::map<std::string, std::string> mArguments;

            void parseCommandLine(int argc, char *argv[]);
            void applyDefault(
                const std::string &argumentKey, const std::string &defaultValue);
            bool trySetEchoMode(bool enabled);
            bool tryAskSafely(std::string message, std::string argumentKey);
            bool tryLoadFromEnv(std::string envVarName, std::string argumentKey);
            bool tryLoadSecret(
                const std::string &envVarName,
                const std::string &argumentKey,
                const std::string &description,
                const std::string &message);

        public:
            /// @brief Execution manifest filename argument key
            static const std::string cConfigArgument;
            /// @brief Extended Vehicle AA manifest filename argument key
            static const std::string cEvConfigArgument;
            /// @brief Diagnostic Manager manifest filename argument key
            static const std::string cDmConfigArgument;
            /// @brief Platform Health Management manifest filename argument key
            static const std::string cPhmConfigArgument;
            /// @brief VCC API key argument key
            static const std::string cApiKeyArgument;
            /// @brief OAuth 2.0 bearer token argument key
            static const std::string cBearerTokenArgument;
            /// @brief Non-interactive (headless) mode argument key
            static const std::string cNonInteractiveArgument;
            /// @brief Bounded run duration (milliseconds) argument key
            static const std::string cRunDurationArgument;
            /// @brief VCC API key environment variable name
            static const std::string cApiKeyEnvVar;
            /// @brief OAuth 2.0 bearer token environment variable name
            static const std::string cBearerTokenEnvVar;
            /// @brief Bounded run duration (milliseconds) environment variable name
            static const std::string cRunDurationEnvVar;
            /// @brief Prefix of the generic environment variables (e.g., 'ADAPTIVE_AUTOSAR_CONFIG')
            static const std::string cEnvVarPrefix;
            /// @brief Command line option prefix (e.g., '--config=...')
            static const std::string cOptionPrefix;

            /// @brief Constructor
            /// @param argc Argument count
            /// @param argv Passed arguments
            /// @param defaultConfigFile Default execution manifest file path
            /// @param extendedVehicleConfigFile Default Extended Vehicle AA manifest file path
            /// @param diagnosticManagerConfigFile Default DM manifest file path
            /// @param healthMonitoringConfigFile Default PHM manifest file path
            ArgumentConfiguration(
                int argc,
                char *argv[],
                std::string defaultConfigFile = "../../configuration/execution_manifest.arxml",
                std::string extendedVehicleConfigFile = "../../configuration/extended_vehicle_manifest.arxml",
                std::string diagnosticManagerConfigFile = "../../configuration/diagnostic_manager_manifest.arxml",
                std::string healthMonitoringConfigFile = "../../configuration/health_monitoring_manifest.arxml");
            ArgumentConfiguration() = delete;

            /// @brief Get the generic environment variable name of an argument key
            /// @param argumentKey Argument key (e.g., 'config')
            /// @return Environment variable name (e.g., 'ADAPTIVE_AUTOSAR_CONFIG')
            static std::string GetEnvVarName(const std::string &argumentKey);

            /// @brief Arguments property getter
            /// @return All the parsed and/or set arguments
            const std::map<std::string, std::string> &GetArguments() const noexcept;

            /// @brief Indicate whether the application runs without a human attached
            /// @details Explicitly set via '--noninteractive' or
            /// 'ADAPTIVE_AUTOSAR_NONINTERACTIVE'; otherwise inferred from
            /// whether the standard input is a terminal.
            /// @return True if the application should not wait for console input
            bool IsNonInteractive() const;

            /// @brief Try to get the bounded run duration set via
            /// '--runduration=<ms>' or 'RUN_DURATION_MS'
            /// @param[out] duration Run duration in milliseconds
            /// @return True if a valid positive duration is set; otherwise false
            bool TryGetRunDuration(std::chrono::milliseconds &duration) const;

            /// @brief Try to load the VCC API key from the environment variable,
            /// falling back to interactive stdin prompt if not set
            /// @param message Fallback message to be shown on the console
            /// @return True if the API key is set correctly; otherwise false
            bool TryAskingVccApiKey(
                std::string message = "Please enter the VCC API key:");

            /// @brief Try to load the OAuth 2.0 bearer token from the environment variable,
            /// falling back to interactive stdin prompt if not set
            /// @param message Fallback message to be shown on the console
            /// @return True if the bearer token is set correctly; otherwise false
            bool TryAskingBearToken(
                std::string message = "Please enter the OAuth 2.0 bearer token:");
        };
    }
}

#endif
