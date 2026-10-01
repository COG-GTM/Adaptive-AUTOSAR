#include <gtest/gtest.h>
#include <cstdlib>
#include <string>
#include <vector>
#include "../../../src/application/helper/argument_configuration.h"

namespace application
{
    namespace helper
    {
        class ArgumentConfigurationTest : public testing::Test
        {
        private:
            std::vector<std::string> mArgumentStrings;
            std::map<std::string, std::pair<bool, std::string>> mOriginalEnvVars;

            void backupAndUnsetEnvVar(const std::string &envVarName)
            {
                const char *_value{std::getenv(envVarName.c_str())};
                if (_value != nullptr)
                {
                    mOriginalEnvVars[envVarName] = std::make_pair(true, std::string(_value));
                }
                else
                {
                    mOriginalEnvVars[envVarName] = std::make_pair(false, std::string());
                }

                unsetenv(envVarName.c_str());
            }

        protected:
            const std::string cDefaultConfig{"default_execution_manifest.arxml"};
            const std::string cDefaultEvConfig{"default_extended_vehicle_manifest.arxml"};
            const std::string cDefaultDmConfig{"default_diagnostic_manager_manifest.arxml"};
            const std::string cDefaultPhmConfig{"default_health_monitoring_manifest.arxml"};

            std::vector<char *> Argv;

            void SetUp() override
            {
                backupAndUnsetEnvVar(ArgumentConfiguration::cApiKeyEnvVar);
                backupAndUnsetEnvVar(ArgumentConfiguration::cBearerTokenEnvVar);
            }

            void TearDown() override
            {
                for (const auto &_envVar : mOriginalEnvVars)
                {
                    if (_envVar.second.first)
                    {
                        setenv(_envVar.first.c_str(), _envVar.second.second.c_str(), 1);
                    }
                    else
                    {
                        unsetenv(_envVar.first.c_str());
                    }
                }
            }

            void SetArguments(std::vector<std::string> arguments)
            {
                mArgumentStrings = std::move(arguments);
                Argv.clear();
                for (auto &_argument : mArgumentStrings)
                {
                    Argv.push_back(&_argument[0]);
                }
                Argv.push_back(nullptr);
            }

            int Argc() const noexcept
            {
                return static_cast<int>(mArgumentStrings.size());
            }

            ArgumentConfiguration CreateConfiguration()
            {
                return ArgumentConfiguration(
                    Argc(),
                    Argv.data(),
                    cDefaultConfig,
                    cDefaultEvConfig,
                    cDefaultDmConfig,
                    cDefaultPhmConfig);
            }

            void ExpectDefaultManifests(const ArgumentConfiguration &configuration)
            {
                const std::map<std::string, std::string> &_arguments{configuration.GetArguments()};
                EXPECT_EQ(_arguments.at(ArgumentConfiguration::cConfigArgument), cDefaultConfig);
                EXPECT_EQ(_arguments.at(ArgumentConfiguration::cEvConfigArgument), cDefaultEvConfig);
                EXPECT_EQ(_arguments.at(ArgumentConfiguration::cDmConfigArgument), cDefaultDmConfig);
                EXPECT_EQ(_arguments.at(ArgumentConfiguration::cPhmConfigArgument), cDefaultPhmConfig);
            }
        };

        TEST_F(ArgumentConfigurationTest, ZeroArgumentCount)
        {
            SetArguments({});

            ArgumentConfiguration _configuration{CreateConfiguration()};

            ExpectDefaultManifests(_configuration);
        }

        TEST_F(ArgumentConfigurationTest, OnlyProgramName)
        {
            SetArguments({"adaptive_autosar"});

            ArgumentConfiguration _configuration{CreateConfiguration()};

            ExpectDefaultManifests(_configuration);
        }

        TEST_F(ArgumentConfigurationTest, MissingArguments)
        {
            const std::vector<std::string> cAllArguments{
                "adaptive_autosar",
                "execution.arxml",
                "extended_vehicle.arxml",
                "diagnostic_manager.arxml"};

            for (std::size_t _count = 2; _count <= cAllArguments.size(); ++_count)
            {
                SetArguments(
                    std::vector<std::string>(
                        cAllArguments.begin(), cAllArguments.begin() + _count));

                ArgumentConfiguration _configuration{CreateConfiguration()};

                SCOPED_TRACE("argc = " + std::to_string(_count));
                ExpectDefaultManifests(_configuration);
            }
        }

        TEST_F(ArgumentConfigurationTest, DefaultConstructorArguments)
        {
            const std::size_t cExpectedSize{4};
            SetArguments({"adaptive_autosar"});

            ArgumentConfiguration _configuration(Argc(), Argv.data());
            const std::map<std::string, std::string> &_arguments{_configuration.GetArguments()};

            EXPECT_EQ(_arguments.size(), cExpectedSize);
            EXPECT_EQ(
                _arguments.at(ArgumentConfiguration::cConfigArgument),
                "../../configuration/execution_manifest.arxml");
            EXPECT_EQ(
                _arguments.at(ArgumentConfiguration::cEvConfigArgument),
                "../../configuration/extended_vehicle_manifest.arxml");
            EXPECT_EQ(
                _arguments.at(ArgumentConfiguration::cDmConfigArgument),
                "../../configuration/diagnostic_manager_manifest.arxml");
            EXPECT_EQ(
                _arguments.at(ArgumentConfiguration::cPhmConfigArgument),
                "../../configuration/health_monitoring_manifest.arxml");
        }

        TEST_F(ArgumentConfigurationTest, ManifestPathMapping)
        {
            const std::string cConfig{"./configuration/execution_manifest.arxml"};
            const std::string cEvConfig{"./configuration/extended_vehicle_manifest.arxml"};
            const std::string cDmConfig{"./configuration/diagnostic_manager_manifest.arxml"};
            const std::string cPhmConfig{"./configuration/health_monitoring_manifest.arxml"};
            const std::size_t cExpectedSize{4};

            SetArguments({"adaptive_autosar", cConfig, cEvConfig, cDmConfig, cPhmConfig});

            ArgumentConfiguration _configuration{CreateConfiguration()};
            const std::map<std::string, std::string> &_arguments{_configuration.GetArguments()};

            EXPECT_EQ(_arguments.size(), cExpectedSize);
            EXPECT_EQ(_arguments.at(ArgumentConfiguration::cConfigArgument), cConfig);
            EXPECT_EQ(_arguments.at(ArgumentConfiguration::cEvConfigArgument), cEvConfig);
            EXPECT_EQ(_arguments.at(ArgumentConfiguration::cDmConfigArgument), cDmConfig);
            EXPECT_EQ(_arguments.at(ArgumentConfiguration::cPhmConfigArgument), cPhmConfig);
        }

        TEST_F(ArgumentConfigurationTest, ExtraArgumentsIgnored)
        {
            const std::string cConfig{"a.arxml"};
            const std::string cEvConfig{"b.arxml"};
            const std::string cDmConfig{"c.arxml"};
            const std::string cPhmConfig{"d.arxml"};
            const std::size_t cExpectedSize{4};

            SetArguments(
                {"adaptive_autosar", cConfig, cEvConfig, cDmConfig, cPhmConfig, "extra.arxml"});

            ArgumentConfiguration _configuration{CreateConfiguration()};
            const std::map<std::string, std::string> &_arguments{_configuration.GetArguments()};

            EXPECT_EQ(_arguments.size(), cExpectedSize);
            EXPECT_EQ(_arguments.at(ArgumentConfiguration::cConfigArgument), cConfig);
            EXPECT_EQ(_arguments.at(ArgumentConfiguration::cEvConfigArgument), cEvConfig);
            EXPECT_EQ(_arguments.at(ArgumentConfiguration::cDmConfigArgument), cDmConfig);
            EXPECT_EQ(_arguments.at(ArgumentConfiguration::cPhmConfigArgument), cPhmConfig);
        }

        TEST_F(ArgumentConfigurationTest, SecretsAbsentByDefault)
        {
            SetArguments({"adaptive_autosar"});

            ArgumentConfiguration _configuration{CreateConfiguration()};
            const std::map<std::string, std::string> &_arguments{_configuration.GetArguments()};

            EXPECT_EQ(_arguments.count(ArgumentConfiguration::cApiKeyArgument), 0);
            EXPECT_EQ(_arguments.count(ArgumentConfiguration::cBearerTokenArgument), 0);
        }

        TEST_F(ArgumentConfigurationTest, VccApiKeyFromEnvironment)
        {
            const std::string cApiKey{"test-vcc-api-key"};
            SetArguments({"adaptive_autosar"});
            setenv(ArgumentConfiguration::cApiKeyEnvVar.c_str(), cApiKey.c_str(), 1);

            ArgumentConfiguration _configuration{CreateConfiguration()};
            bool _succeed{_configuration.TryAskingVccApiKey()};

            EXPECT_TRUE(_succeed);
            EXPECT_EQ(
                _configuration.GetArguments().at(ArgumentConfiguration::cApiKeyArgument),
                cApiKey);
            ExpectDefaultManifests(_configuration);
        }

        TEST_F(ArgumentConfigurationTest, BearerTokenFromEnvironment)
        {
            const std::string cBearerToken{"test-bearer-token"};
            SetArguments({"adaptive_autosar"});
            setenv(ArgumentConfiguration::cBearerTokenEnvVar.c_str(), cBearerToken.c_str(), 1);

            ArgumentConfiguration _configuration{CreateConfiguration()};
            bool _succeed{_configuration.TryAskingBearToken()};

            EXPECT_TRUE(_succeed);
            EXPECT_EQ(
                _configuration.GetArguments().at(ArgumentConfiguration::cBearerTokenArgument),
                cBearerToken);
            ExpectDefaultManifests(_configuration);
        }
    }
}
