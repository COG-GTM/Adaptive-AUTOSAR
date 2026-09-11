#include <cstdlib>
#include <vector>
#include <gtest/gtest.h>
#include "../../../src/application/helper/argument_configuration.h"

namespace application
{
    namespace helper
    {
        class ArgumentConfigurationTest : public testing::Test
        {
        private:
            const std::vector<std::string> cManagedEnvVars{
                ArgumentConfiguration::cApiKeyEnvVar,
                ArgumentConfiguration::cBearerTokenEnvVar,
                ArgumentConfiguration::cRunDurationEnvVar,
                ArgumentConfiguration::GetEnvVarName(ArgumentConfiguration::cConfigArgument),
                ArgumentConfiguration::GetEnvVarName(ArgumentConfiguration::cEvConfigArgument),
                ArgumentConfiguration::GetEnvVarName(ArgumentConfiguration::cDmConfigArgument),
                ArgumentConfiguration::GetEnvVarName(ArgumentConfiguration::cPhmConfigArgument),
                ArgumentConfiguration::GetEnvVarName(ArgumentConfiguration::cNonInteractiveArgument)};

            std::vector<std::string> mArgvStorage;
            std::vector<char *> mArgv;

        protected:
            const std::string cDefaultConfig{"default_em.arxml"};
            const std::string cDefaultEvConfig{"default_ev.arxml"};
            const std::string cDefaultDmConfig{"default_dm.arxml"};
            const std::string cDefaultPhmConfig{"default_phm.arxml"};

            void SetUp() override
            {
                clearEnv();
            }

            void TearDown() override
            {
                clearEnv();
            }

            void clearEnv()
            {
                for (const auto &_envVar : cManagedEnvVars)
                {
                    unsetenv(_envVar.c_str());
                }
            }

            void setEnv(const std::string &name, const std::string &value)
            {
                setenv(name.c_str(), value.c_str(), 1);
            }

            ArgumentConfiguration create(std::vector<std::string> arguments)
            {
                mArgvStorage = std::move(arguments);
                mArgvStorage.insert(mArgvStorage.begin(), "adaptive_autosar");

                mArgv.clear();
                for (auto &_argument : mArgvStorage)
                {
                    mArgv.push_back(const_cast<char *>(_argument.c_str()));
                }

                return ArgumentConfiguration(
                    static_cast<int>(mArgv.size()),
                    mArgv.data(),
                    cDefaultConfig,
                    cDefaultEvConfig,
                    cDefaultDmConfig,
                    cDefaultPhmConfig);
            }
        };

        TEST_F(ArgumentConfigurationTest, GetEnvVarName)
        {
            EXPECT_EQ(
                "ADAPTIVE_AUTOSAR_CONFIG",
                ArgumentConfiguration::GetEnvVarName(ArgumentConfiguration::cConfigArgument));
            EXPECT_EQ(
                "ADAPTIVE_AUTOSAR_PHMCONFIG",
                ArgumentConfiguration::GetEnvVarName(ArgumentConfiguration::cPhmConfigArgument));
        }

        TEST_F(ArgumentConfigurationTest, DefaultsWithoutArguments)
        {
            ArgumentConfiguration _configuration{create({})};
            const auto &_arguments{_configuration.GetArguments()};

            EXPECT_EQ(cDefaultConfig, _arguments.at(ArgumentConfiguration::cConfigArgument));
            EXPECT_EQ(cDefaultEvConfig, _arguments.at(ArgumentConfiguration::cEvConfigArgument));
            EXPECT_EQ(cDefaultDmConfig, _arguments.at(ArgumentConfiguration::cDmConfigArgument));
            EXPECT_EQ(cDefaultPhmConfig, _arguments.at(ArgumentConfiguration::cPhmConfigArgument));
            EXPECT_EQ(0, _arguments.count(ArgumentConfiguration::cApiKeyArgument));
            EXPECT_EQ(0, _arguments.count(ArgumentConfiguration::cBearerTokenArgument));
        }

        TEST_F(ArgumentConfigurationTest, PositionalArguments)
        {
            ArgumentConfiguration _configuration{
                create({"em.arxml", "ev.arxml", "dm.arxml", "phm.arxml"})};
            const auto &_arguments{_configuration.GetArguments()};

            EXPECT_EQ("em.arxml", _arguments.at(ArgumentConfiguration::cConfigArgument));
            EXPECT_EQ("ev.arxml", _arguments.at(ArgumentConfiguration::cEvConfigArgument));
            EXPECT_EQ("dm.arxml", _arguments.at(ArgumentConfiguration::cDmConfigArgument));
            EXPECT_EQ("phm.arxml", _arguments.at(ArgumentConfiguration::cPhmConfigArgument));
        }

        TEST_F(ArgumentConfigurationTest, PartialPositionalArgumentsFallBackToDefaults)
        {
            ArgumentConfiguration _configuration{create({"em.arxml", "ev.arxml"})};
            const auto &_arguments{_configuration.GetArguments()};

            EXPECT_EQ("em.arxml", _arguments.at(ArgumentConfiguration::cConfigArgument));
            EXPECT_EQ("ev.arxml", _arguments.at(ArgumentConfiguration::cEvConfigArgument));
            EXPECT_EQ(cDefaultDmConfig, _arguments.at(ArgumentConfiguration::cDmConfigArgument));
            EXPECT_EQ(cDefaultPhmConfig, _arguments.at(ArgumentConfiguration::cPhmConfigArgument));
        }

        TEST_F(ArgumentConfigurationTest, KeyValueOptions)
        {
            ArgumentConfiguration _configuration{
                create({"--dmconfig=dm.arxml", "--runduration=1500"})};
            const auto &_arguments{_configuration.GetArguments()};

            EXPECT_EQ("dm.arxml", _arguments.at(ArgumentConfiguration::cDmConfigArgument));
            EXPECT_EQ(cDefaultConfig, _arguments.at(ArgumentConfiguration::cConfigArgument));
            EXPECT_EQ("1500", _arguments.at(ArgumentConfiguration::cRunDurationArgument));
        }

        TEST_F(ArgumentConfigurationTest, KeyValueOptionOverridesPositional)
        {
            ArgumentConfiguration _before{
                create({"--config=option.arxml", "positional.arxml"})};
            EXPECT_EQ(
                "option.arxml",
                _before.GetArguments().at(ArgumentConfiguration::cConfigArgument));

            ArgumentConfiguration _after{
                create({"positional.arxml", "--config=option.arxml"})};
            EXPECT_EQ(
                "option.arxml",
                _after.GetArguments().at(ArgumentConfiguration::cConfigArgument));
        }

        TEST_F(ArgumentConfigurationTest, EnvVarFallbackForManifestPaths)
        {
            setEnv(
                ArgumentConfiguration::GetEnvVarName(ArgumentConfiguration::cEvConfigArgument),
                "env_ev.arxml");
            setEnv(
                ArgumentConfiguration::GetEnvVarName(ArgumentConfiguration::cConfigArgument),
                "env_em.arxml");

            ArgumentConfiguration _configuration{create({"cli_em.arxml"})};
            const auto &_arguments{_configuration.GetArguments()};

            // CLI wins over env, env wins over default
            EXPECT_EQ("cli_em.arxml", _arguments.at(ArgumentConfiguration::cConfigArgument));
            EXPECT_EQ("env_ev.arxml", _arguments.at(ArgumentConfiguration::cEvConfigArgument));
            EXPECT_EQ(cDefaultDmConfig, _arguments.at(ArgumentConfiguration::cDmConfigArgument));
        }

        TEST_F(ArgumentConfigurationTest, EmptyEnvVarIsIgnored)
        {
            setEnv(
                ArgumentConfiguration::GetEnvVarName(ArgumentConfiguration::cConfigArgument),
                "");

            ArgumentConfiguration _configuration{create({})};

            EXPECT_EQ(
                cDefaultConfig,
                _configuration.GetArguments().at(ArgumentConfiguration::cConfigArgument));
        }

        TEST_F(ArgumentConfigurationTest, SecretsFromEnv)
        {
            setEnv(ArgumentConfiguration::cApiKeyEnvVar, "api-key");
            setEnv(ArgumentConfiguration::cBearerTokenEnvVar, "bearer-token");

            ArgumentConfiguration _configuration{create({"--noninteractive"})};

            EXPECT_TRUE(_configuration.TryAskingVccApiKey());
            EXPECT_TRUE(_configuration.TryAskingBearToken());

            const auto &_arguments{_configuration.GetArguments()};
            EXPECT_EQ("api-key", _arguments.at(ArgumentConfiguration::cApiKeyArgument));
            EXPECT_EQ("bearer-token", _arguments.at(ArgumentConfiguration::cBearerTokenArgument));
        }

        TEST_F(ArgumentConfigurationTest, MissingSecretsFailInNonInteractiveMode)
        {
            ArgumentConfiguration _configuration{create({"--noninteractive"})};

            EXPECT_FALSE(_configuration.TryAskingVccApiKey());
            EXPECT_FALSE(_configuration.TryAskingBearToken());
            EXPECT_EQ(0, _configuration.GetArguments().count(ArgumentConfiguration::cApiKeyArgument));
        }

        TEST_F(ArgumentConfigurationTest, EmptySecretEnvVarFailsInNonInteractiveMode)
        {
            setEnv(ArgumentConfiguration::cApiKeyEnvVar, "");
            ArgumentConfiguration _configuration{create({"--noninteractive"})};

            EXPECT_FALSE(_configuration.TryAskingVccApiKey());
        }

        TEST_F(ArgumentConfigurationTest, NonInteractiveFlag)
        {
            EXPECT_TRUE(create({"--noninteractive"}).IsNonInteractive());
            EXPECT_TRUE(create({"--noninteractive=1"}).IsNonInteractive());
            EXPECT_FALSE(create({"--noninteractive=0"}).IsNonInteractive());
            EXPECT_FALSE(create({"--noninteractive=false"}).IsNonInteractive());
        }

        TEST_F(ArgumentConfigurationTest, NonInteractiveEnvVar)
        {
            const std::string cEnvVar{
                ArgumentConfiguration::GetEnvVarName(
                    ArgumentConfiguration::cNonInteractiveArgument)};

            setEnv(cEnvVar, "1");
            EXPECT_TRUE(create({}).IsNonInteractive());

            setEnv(cEnvVar, "0");
            EXPECT_FALSE(create({}).IsNonInteractive());

            // CLI flag has precedence over the environment variable
            EXPECT_TRUE(create({"--noninteractive"}).IsNonInteractive());
        }

        TEST_F(ArgumentConfigurationTest, RunDurationFromOption)
        {
            std::chrono::milliseconds _duration;
            ArgumentConfiguration _configuration{create({"--runduration=2500"})};

            ASSERT_TRUE(_configuration.TryGetRunDuration(_duration));
            EXPECT_EQ(std::chrono::milliseconds{2500}, _duration);
        }

        TEST_F(ArgumentConfigurationTest, RunDurationFromEnv)
        {
            setEnv(ArgumentConfiguration::cRunDurationEnvVar, "750");

            std::chrono::milliseconds _duration;
            ArgumentConfiguration _configuration{create({})};

            ASSERT_TRUE(_configuration.TryGetRunDuration(_duration));
            EXPECT_EQ(std::chrono::milliseconds{750}, _duration);
        }

        TEST_F(ArgumentConfigurationTest, RunDurationOptionOverridesEnv)
        {
            setEnv(ArgumentConfiguration::cRunDurationEnvVar, "750");

            std::chrono::milliseconds _duration;
            ArgumentConfiguration _configuration{create({"--runduration=100"})};

            ASSERT_TRUE(_configuration.TryGetRunDuration(_duration));
            EXPECT_EQ(std::chrono::milliseconds{100}, _duration);
        }

        TEST_F(ArgumentConfigurationTest, InvalidRunDuration)
        {
            std::chrono::milliseconds _duration;

            EXPECT_FALSE(create({}).TryGetRunDuration(_duration));
            EXPECT_FALSE(create({"--runduration=0"}).TryGetRunDuration(_duration));
            EXPECT_FALSE(create({"--runduration=-5"}).TryGetRunDuration(_duration));
            EXPECT_FALSE(create({"--runduration=abc"}).TryGetRunDuration(_duration));
            EXPECT_FALSE(create({"--runduration=12ms"}).TryGetRunDuration(_duration));
            EXPECT_FALSE(create({"--runduration="}).TryGetRunDuration(_duration));
        }
    }
}
