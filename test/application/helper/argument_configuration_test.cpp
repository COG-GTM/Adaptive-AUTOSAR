#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <gtest/gtest.h>
#include "../../../src/application/helper/argument_configuration.h"

namespace application
{
    namespace helper
    {
        class ArgumentConfigurationTest : public testing::Test
        {
        private:
            bool mHadApiKey;
            std::string mApiKey;
            bool mHadBearerToken;
            std::string mBearerToken;
            int mStdinBackup;

            static bool tryBackupEnv(const std::string &name, std::string &value)
            {
                const char *_value{std::getenv(name.c_str())};
                if (_value != nullptr)
                {
                    value = _value;
                    return true;
                }
                return false;
            }

            static void restoreEnv(
                const std::string &name, bool hadValue, const std::string &value)
            {
                if (hadValue)
                {
                    setenv(name.c_str(), value.c_str(), 1);
                }
                else
                {
                    unsetenv(name.c_str());
                }
            }

        protected:
            void SetUp() override
            {
                mHadApiKey = tryBackupEnv(
                    ArgumentConfiguration::cApiKeyEnvVar, mApiKey);
                mHadBearerToken = tryBackupEnv(
                    ArgumentConfiguration::cBearerTokenEnvVar, mBearerToken);
                unsetenv(ArgumentConfiguration::cApiKeyEnvVar.c_str());
                unsetenv(ArgumentConfiguration::cBearerTokenEnvVar.c_str());

                // Redirect stdin to a non-terminal so the interactive fallback never blocks
                mStdinBackup = dup(STDIN_FILENO);
                const int cNullFd{open("/dev/null", O_RDONLY)};
                dup2(cNullFd, STDIN_FILENO);
                close(cNullFd);
            }

            void TearDown() override
            {
                dup2(mStdinBackup, STDIN_FILENO);
                close(mStdinBackup);

                restoreEnv(
                    ArgumentConfiguration::cApiKeyEnvVar, mHadApiKey, mApiKey);
                restoreEnv(
                    ArgumentConfiguration::cBearerTokenEnvVar, mHadBearerToken, mBearerToken);
            }
        };

        TEST_F(ArgumentConfigurationTest, DefaultManifestPaths)
        {
            char cExecutable[]{"adaptive_autosar"};
            char *_argv[]{cExecutable};

            ArgumentConfiguration _configuration(1, _argv);
            const auto &cArguments{_configuration.GetArguments()};

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
            EXPECT_EQ(4, cArguments.size());
        }

        TEST_F(ArgumentConfigurationTest, CustomDefaultManifestPaths)
        {
            char cExecutable[]{"adaptive_autosar"};
            char *_argv[]{cExecutable};

            ArgumentConfiguration _configuration(
                1, _argv, "em.arxml", "ev.arxml", "dm.arxml", "phm.arxml");
            const auto &cArguments{_configuration.GetArguments()};

            EXPECT_EQ("em.arxml", cArguments.at(ArgumentConfiguration::cConfigArgument));
            EXPECT_EQ("ev.arxml", cArguments.at(ArgumentConfiguration::cEvConfigArgument));
            EXPECT_EQ("dm.arxml", cArguments.at(ArgumentConfiguration::cDmConfigArgument));
            EXPECT_EQ("phm.arxml", cArguments.at(ArgumentConfiguration::cPhmConfigArgument));
        }

        TEST_F(ArgumentConfigurationTest, InsufficientArgvFallsBackToDefaults)
        {
            char cExecutable[]{"adaptive_autosar"};
            char cEm[]{"/tmp/em.arxml"};
            char cEv[]{"/tmp/ev.arxml"};
            char cDm[]{"/tmp/dm.arxml"};
            char *_argv[]{cExecutable, cEm, cEv, cDm};

            ArgumentConfiguration _configuration(
                4, _argv, "em.arxml", "ev.arxml", "dm.arxml", "phm.arxml");
            const auto &cArguments{_configuration.GetArguments()};

            EXPECT_EQ("em.arxml", cArguments.at(ArgumentConfiguration::cConfigArgument));
            EXPECT_EQ("ev.arxml", cArguments.at(ArgumentConfiguration::cEvConfigArgument));
            EXPECT_EQ("dm.arxml", cArguments.at(ArgumentConfiguration::cDmConfigArgument));
            EXPECT_EQ("phm.arxml", cArguments.at(ArgumentConfiguration::cPhmConfigArgument));
        }

        TEST_F(ArgumentConfigurationTest, ArgvManifestPaths)
        {
            char cExecutable[]{"adaptive_autosar"};
            char cEm[]{"/tmp/em.arxml"};
            char cEv[]{"/tmp/ev.arxml"};
            char cDm[]{"/tmp/dm.arxml"};
            char cPhm[]{"/tmp/phm.arxml"};
            char *_argv[]{cExecutable, cEm, cEv, cDm, cPhm};

            ArgumentConfiguration _configuration(5, _argv);
            const auto &cArguments{_configuration.GetArguments()};

            EXPECT_EQ("/tmp/em.arxml", cArguments.at(ArgumentConfiguration::cConfigArgument));
            EXPECT_EQ("/tmp/ev.arxml", cArguments.at(ArgumentConfiguration::cEvConfigArgument));
            EXPECT_EQ("/tmp/dm.arxml", cArguments.at(ArgumentConfiguration::cDmConfigArgument));
            EXPECT_EQ("/tmp/phm.arxml", cArguments.at(ArgumentConfiguration::cPhmConfigArgument));
            EXPECT_EQ(cArguments.end(), cArguments.find(ArgumentConfiguration::cApiKeyArgument));
            EXPECT_EQ(cArguments.end(), cArguments.find(ArgumentConfiguration::cBearerTokenArgument));
        }

        TEST_F(ArgumentConfigurationTest, VccApiKeyFromEnv)
        {
            const std::string cApiKey{"test-vcc-api-key"};
            setenv(ArgumentConfiguration::cApiKeyEnvVar.c_str(), cApiKey.c_str(), 1);

            char cExecutable[]{"adaptive_autosar"};
            char *_argv[]{cExecutable};
            ArgumentConfiguration _configuration(1, _argv);

            EXPECT_TRUE(_configuration.TryAskingVccApiKey());
            EXPECT_EQ(
                cApiKey,
                _configuration.GetArguments().at(ArgumentConfiguration::cApiKeyArgument));
        }

        TEST_F(ArgumentConfigurationTest, BearerTokenFromEnv)
        {
            const std::string cBearerToken{"test-bearer-token"};
            setenv(ArgumentConfiguration::cBearerTokenEnvVar.c_str(), cBearerToken.c_str(), 1);

            char cExecutable[]{"adaptive_autosar"};
            char *_argv[]{cExecutable};
            ArgumentConfiguration _configuration(1, _argv);

            EXPECT_TRUE(_configuration.TryAskingBearToken());
            EXPECT_EQ(
                cBearerToken,
                _configuration.GetArguments().at(ArgumentConfiguration::cBearerTokenArgument));
        }

        TEST_F(ArgumentConfigurationTest, MissingEnvFallsBackToStdin)
        {
            char cExecutable[]{"adaptive_autosar"};
            char *_argv[]{cExecutable};
            ArgumentConfiguration _configuration(1, _argv);

            // stdin is not a terminal, so the interactive fallback must fail gracefully
            EXPECT_FALSE(_configuration.TryAskingVccApiKey());
            EXPECT_FALSE(_configuration.TryAskingBearToken());

            const auto &cArguments{_configuration.GetArguments()};
            EXPECT_EQ(cArguments.end(), cArguments.find(ArgumentConfiguration::cApiKeyArgument));
            EXPECT_EQ(cArguments.end(), cArguments.find(ArgumentConfiguration::cBearerTokenArgument));
        }

        TEST_F(ArgumentConfigurationTest, EmptyEnvFallsBackToStdin)
        {
            setenv(ArgumentConfiguration::cApiKeyEnvVar.c_str(), "", 1);
            setenv(ArgumentConfiguration::cBearerTokenEnvVar.c_str(), "", 1);

            char cExecutable[]{"adaptive_autosar"};
            char *_argv[]{cExecutable};
            ArgumentConfiguration _configuration(1, _argv);

            EXPECT_FALSE(_configuration.TryAskingVccApiKey());
            EXPECT_FALSE(_configuration.TryAskingBearToken());
        }
    }
}
