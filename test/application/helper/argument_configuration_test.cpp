#include <gtest/gtest.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#include "../../../src/application/helper/argument_configuration.h"
#include "./testable_fixture.h"

namespace application
{
    namespace helper
    {
        class ArgumentConfigurationTest : public testing::Test
        {
        protected:
            char mProgram[4]{"app"};

            static void clearEnvironment()
            {
                unsetenv(ArgumentConfiguration::cApiKeyEnvVar.c_str());
                unsetenv(ArgumentConfiguration::cBearerTokenEnvVar.c_str());
            }

            void SetUp() override
            {
                clearEnvironment();
            }

            void TearDown() override
            {
                clearEnvironment();
            }
        };

        TEST_F(ArgumentConfigurationTest, ExplicitManifestArguments)
        {
            char cConfig[] = "execution.arxml";
            char cEvConfig[] = "ev.arxml";
            char cDmConfig[] = "dm.arxml";
            char cPhmConfig[] = "phm.arxml";
            char *_argv[] = {mProgram, cConfig, cEvConfig, cDmConfig, cPhmConfig};

            ArgumentConfiguration _configuration(5, _argv);
            const auto &cArguments{_configuration.GetArguments()};

            EXPECT_EQ(4, cArguments.size());
            EXPECT_EQ(cConfig, cArguments.at(ArgumentConfiguration::cConfigArgument));
            EXPECT_EQ(cEvConfig, cArguments.at(ArgumentConfiguration::cEvConfigArgument));
            EXPECT_EQ(cDmConfig, cArguments.at(ArgumentConfiguration::cDmConfigArgument));
            EXPECT_EQ(cPhmConfig, cArguments.at(ArgumentConfiguration::cPhmConfigArgument));
        }

        TEST_F(ArgumentConfigurationTest, ExtraArgumentsAreIgnored)
        {
            char cConfig[] = "execution.arxml";
            char cEvConfig[] = "ev.arxml";
            char cDmConfig[] = "dm.arxml";
            char cPhmConfig[] = "phm.arxml";
            char cExtra[] = "extra";
            char *_argv[] = {mProgram, cConfig, cEvConfig, cDmConfig, cPhmConfig, cExtra};

            ArgumentConfiguration _configuration(6, _argv);

            EXPECT_EQ(4, _configuration.GetArguments().size());
            EXPECT_EQ(
                cPhmConfig,
                _configuration.GetArguments().at(ArgumentConfiguration::cPhmConfigArgument));
        }

        TEST_F(ArgumentConfigurationTest, DefaultManifestPaths)
        {
            char *_argv[] = {mProgram};

            ArgumentConfiguration _configuration(1, _argv);
            const auto &cArguments{_configuration.GetArguments()};

            EXPECT_EQ(4, cArguments.size());
            EXPECT_EQ(
                "../../configuration/execution_manifest.arxml",
                cArguments.at(ArgumentConfiguration::cConfigArgument));
            EXPECT_EQ(
                "../../configuration/extended_vehicle_manifest.arxml",
                cArguments.at(ArgumentConfiguration::cEvConfigArgument));
            EXPECT_EQ(
                "../../configuration/diagnostic_manager_manifest.arxml",
                cArguments.at(ArgumentConfiguration::cDmConfigArgument));
            EXPECT_EQ(
                "../../configuration/health_monitoring_manifest.arxml",
                cArguments.at(ArgumentConfiguration::cPhmConfigArgument));
            EXPECT_EQ(0, cArguments.count(ArgumentConfiguration::cApiKeyArgument));
            EXPECT_EQ(0, cArguments.count(ArgumentConfiguration::cBearerTokenArgument));
        }

        TEST_F(ArgumentConfigurationTest, IncompleteArgumentsFallBackToDefaults)
        {
            char cConfig[] = "execution.arxml";
            char cEvConfig[] = "ev.arxml";
            char *_argv[] = {mProgram, cConfig, cEvConfig};

            ArgumentConfiguration _configuration(
                3, _argv, "default_em", "default_ev", "default_dm", "default_phm");
            const auto &cArguments{_configuration.GetArguments()};

            EXPECT_EQ("default_em", cArguments.at(ArgumentConfiguration::cConfigArgument));
            EXPECT_EQ("default_ev", cArguments.at(ArgumentConfiguration::cEvConfigArgument));
            EXPECT_EQ("default_dm", cArguments.at(ArgumentConfiguration::cDmConfigArgument));
            EXPECT_EQ("default_phm", cArguments.at(ArgumentConfiguration::cPhmConfigArgument));
        }

        TEST_F(ArgumentConfigurationTest, LoadVccApiKeyFromEnvironment)
        {
            const std::string cApiKey{"test-api-key"};
            setenv(ArgumentConfiguration::cApiKeyEnvVar.c_str(), cApiKey.c_str(), 1);
            char *_argv[] = {mProgram};
            ArgumentConfiguration _configuration(1, _argv);
            fixture::StdoutCapture _stdout;

            EXPECT_TRUE(_configuration.TryAskingVccApiKey());
            EXPECT_EQ(
                cApiKey,
                _configuration.GetArguments().at(ArgumentConfiguration::cApiKeyArgument));
            EXPECT_TRUE(_stdout.Contains("VCC API key loaded from environment variable."));
            EXPECT_FALSE(_stdout.Contains(cApiKey));
        }

        TEST_F(ArgumentConfigurationTest, LoadBearerTokenFromEnvironment)
        {
            const std::string cBearerToken{"test-bearer-token"};
            setenv(ArgumentConfiguration::cBearerTokenEnvVar.c_str(), cBearerToken.c_str(), 1);
            char *_argv[] = {mProgram};
            ArgumentConfiguration _configuration(1, _argv);
            fixture::StdoutCapture _stdout;

            EXPECT_TRUE(_configuration.TryAskingBearToken());
            EXPECT_EQ(
                cBearerToken,
                _configuration.GetArguments().at(ArgumentConfiguration::cBearerTokenArgument));
            EXPECT_TRUE(_stdout.Contains("OAuth 2.0 bearer token loaded from environment variable."));
            EXPECT_FALSE(_stdout.Contains(cBearerToken));
        }

        TEST_F(ArgumentConfigurationTest, EmptyEnvironmentVariableFallsBackToPrompt)
        {
            // The interactive prompt cannot be answered without a TTY,
            // so stdin is redirected to /dev/null to make the fallback fail deterministically.
            setenv(ArgumentConfiguration::cApiKeyEnvVar.c_str(), "", 1);
            setenv(ArgumentConfiguration::cBearerTokenEnvVar.c_str(), "", 1);
            char *_argv[] = {mProgram};
            ArgumentConfiguration _configuration(1, _argv);

            const int cOriginalStdin{dup(STDIN_FILENO)};
            const int cNullDescriptor{open("/dev/null", O_RDONLY)};
            ASSERT_GE(cNullDescriptor, 0);
            dup2(cNullDescriptor, STDIN_FILENO);
            close(cNullDescriptor);

            bool _apiKeyResult;
            bool _bearerTokenResult;
            {
                fixture::StdoutCapture _stdout;
                _apiKeyResult = _configuration.TryAskingVccApiKey("Enter the API key:");
                _bearerTokenResult = _configuration.TryAskingBearToken("Enter the token:");
                EXPECT_TRUE(_stdout.Contains("Enter the API key:"));
                EXPECT_TRUE(_stdout.Contains("Enter the token:"));
            }

            dup2(cOriginalStdin, STDIN_FILENO);
            close(cOriginalStdin);

            EXPECT_FALSE(_apiKeyResult);
            EXPECT_FALSE(_bearerTokenResult);
            EXPECT_EQ(
                0, _configuration.GetArguments().count(ArgumentConfiguration::cApiKeyArgument));
            EXPECT_EQ(
                0, _configuration.GetArguments().count(ArgumentConfiguration::cBearerTokenArgument));
        }
    }
}
