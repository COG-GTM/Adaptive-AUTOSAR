#include <gtest/gtest.h>
#include <cstdlib>
#include <unistd.h>
#include "../../../src/application/helper/argument_configuration.h"

namespace application
{
    namespace helper
    {
        class ArgumentConfigurationTest : public testing::Test
        {
        private:
            bool mHadApiKey{false};
            bool mHadBearerToken{false};
            std::string mOriginalApiKey;
            std::string mOriginalBearerToken;

            static bool tryBackup(const std::string &name, std::string &value)
            {
                const char *_value{std::getenv(name.c_str())};
                if (_value != nullptr)
                {
                    value = _value;
                    return true;
                }

                return false;
            }

            static void restore(const std::string &name, bool existed, const std::string &value)
            {
                if (existed)
                {
                    setenv(name.c_str(), value.c_str(), 1);
                }
                else
                {
                    unsetenv(name.c_str());
                }
            }

        protected:
            char cProgramName[32]{"adaptive_autosar"};
            char *cArgv[1]{cProgramName};

            void SetUp() override
            {
                mHadApiKey = tryBackup(ArgumentConfiguration::cApiKeyEnvVar, mOriginalApiKey);
                mHadBearerToken = tryBackup(ArgumentConfiguration::cBearerTokenEnvVar, mOriginalBearerToken);
                unsetenv(ArgumentConfiguration::cApiKeyEnvVar.c_str());
                unsetenv(ArgumentConfiguration::cBearerTokenEnvVar.c_str());
            }

            void TearDown() override
            {
                restore(ArgumentConfiguration::cApiKeyEnvVar, mHadApiKey, mOriginalApiKey);
                restore(ArgumentConfiguration::cBearerTokenEnvVar, mHadBearerToken, mOriginalBearerToken);
            }
        };

        TEST_F(ArgumentConfigurationTest, DefaultArguments)
        {
            const std::string cConfig{"config.arxml"};
            const std::string cEvConfig{"ev.arxml"};
            const std::string cDmConfig{"dm.arxml"};
            const std::string cPhmConfig{"phm.arxml"};

            ArgumentConfiguration _configuration(
                1, cArgv, cConfig, cEvConfig, cDmConfig, cPhmConfig);
            const auto &cArguments{_configuration.GetArguments()};

            EXPECT_EQ(cConfig, cArguments.at(ArgumentConfiguration::cConfigArgument));
            EXPECT_EQ(cEvConfig, cArguments.at(ArgumentConfiguration::cEvConfigArgument));
            EXPECT_EQ(cDmConfig, cArguments.at(ArgumentConfiguration::cDmConfigArgument));
            EXPECT_EQ(cPhmConfig, cArguments.at(ArgumentConfiguration::cPhmConfigArgument));
            EXPECT_EQ(0, cArguments.count(ArgumentConfiguration::cApiKeyArgument));
            EXPECT_EQ(0, cArguments.count(ArgumentConfiguration::cBearerTokenArgument));
        }

        TEST_F(ArgumentConfigurationTest, CommandLineArguments)
        {
            char _config[]{"/path/execution_manifest.arxml"};
            char _evConfig[]{"/path/extended_vehicle_manifest.arxml"};
            char _dmConfig[]{"/path/diagnostic_manager_manifest.arxml"};
            char _phmConfig[]{"/path/health_monitoring_manifest.arxml"};
            char *_argv[]{cProgramName, _config, _evConfig, _dmConfig, _phmConfig};
            const int cArgc{5};

            ArgumentConfiguration _configuration(cArgc, _argv);
            const auto &cArguments{_configuration.GetArguments()};

            EXPECT_EQ(_config, cArguments.at(ArgumentConfiguration::cConfigArgument));
            EXPECT_EQ(_evConfig, cArguments.at(ArgumentConfiguration::cEvConfigArgument));
            EXPECT_EQ(_dmConfig, cArguments.at(ArgumentConfiguration::cDmConfigArgument));
            EXPECT_EQ(_phmConfig, cArguments.at(ArgumentConfiguration::cPhmConfigArgument));
        }

        TEST_F(ArgumentConfigurationTest, InsufficientCommandLineArguments)
        {
            const std::string cDefaultConfig{"default.arxml"};
            char _config[]{"/path/execution_manifest.arxml"};
            char *_argv[]{cProgramName, _config};
            const int cArgc{2};

            ArgumentConfiguration _configuration(cArgc, _argv, cDefaultConfig);

            EXPECT_EQ(
                cDefaultConfig,
                _configuration.GetArguments().at(ArgumentConfiguration::cConfigArgument));
        }

        TEST_F(ArgumentConfigurationTest, VccApiKeyFromEnvironment)
        {
            const std::string cApiKey{"test-vcc-api-key"};
            setenv(ArgumentConfiguration::cApiKeyEnvVar.c_str(), cApiKey.c_str(), 1);

            ArgumentConfiguration _configuration(1, cArgv);

            EXPECT_TRUE(_configuration.TryAskingVccApiKey());
            EXPECT_EQ(
                cApiKey,
                _configuration.GetArguments().at(ArgumentConfiguration::cApiKeyArgument));
        }

        TEST_F(ArgumentConfigurationTest, BearerTokenFromEnvironment)
        {
            const std::string cBearerToken{"test-bearer-token"};
            setenv(ArgumentConfiguration::cBearerTokenEnvVar.c_str(), cBearerToken.c_str(), 1);

            ArgumentConfiguration _configuration(1, cArgv);

            EXPECT_TRUE(_configuration.TryAskingBearToken());
            EXPECT_EQ(
                cBearerToken,
                _configuration.GetArguments().at(ArgumentConfiguration::cBearerTokenArgument));
        }

        TEST_F(ArgumentConfigurationTest, EnvironmentVariablesAreIndependent)
        {
            const std::string cApiKey{"only-api-key"};
            setenv(ArgumentConfiguration::cApiKeyEnvVar.c_str(), cApiKey.c_str(), 1);

            ArgumentConfiguration _configuration(1, cArgv);

            EXPECT_TRUE(_configuration.TryAskingVccApiKey());
            EXPECT_EQ(0, _configuration.GetArguments().count(ArgumentConfiguration::cBearerTokenArgument));
        }

        TEST_F(ArgumentConfigurationTest, EmptyEnvironmentVariableFallsBack)
        {
            if (isatty(STDIN_FILENO))
            {
                GTEST_SKIP() << "The fallback path would prompt on an interactive terminal.";
            }

            setenv(ArgumentConfiguration::cApiKeyEnvVar.c_str(), "", 1);
            setenv(ArgumentConfiguration::cBearerTokenEnvVar.c_str(), "", 1);

            ArgumentConfiguration _configuration(1, cArgv);

            // Without a terminal, the interactive fallback cannot disable echo and fails.
            EXPECT_FALSE(_configuration.TryAskingVccApiKey());
            EXPECT_FALSE(_configuration.TryAskingBearToken());
            EXPECT_EQ(0, _configuration.GetArguments().count(ArgumentConfiguration::cApiKeyArgument));
            EXPECT_EQ(0, _configuration.GetArguments().count(ArgumentConfiguration::cBearerTokenArgument));
        }
    }
}
