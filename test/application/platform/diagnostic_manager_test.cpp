#include <gtest/gtest.h>
#include "../../../src/application/helper/argument_configuration.h"
#include "../../../src/application/platform/diagnostic_manager.h"
#include "../helper/testable_fixture.h"

namespace application
{
    namespace platform
    {
        class DiagnosticManagerTest : public testing::Test
        {
        protected:
            fixture::StdoutCapture Stdout;
            fixture::TemporaryDirectory Directory;
            AsyncBsdSocketLib::Poller Poller;
            fixture::PollingThread PollingThread;
            std::unique_ptr<DiagnosticManager> Manager;

            DiagnosticManagerTest() : PollingThread(&Poller)
            {
            }

            void SetUp() override
            {
                Manager.reset(new DiagnosticManager(&Poller));
            }

            void TearDown() override
            {
                Manager->Terminate();
                PollingThread.Stop();
                Manager.reset();
            }

            std::map<std::string, std::string> GetArguments(
                const std::map<std::string, std::string> &replacements = {})
            {
                // Isolate the service discovery from other tests and running instances
                std::map<std::string, std::string> _replacements(replacements);
                if (_replacements.empty())
                {
                    _replacements["<PORT-NUMBER>5555</PORT-NUMBER>"] =
                        "<PORT-NUMBER>" +
                        std::to_string(fixture::GetFreePort(SOCK_DGRAM)) +
                        "</PORT-NUMBER>";
                }

                return {{helper::ArgumentConfiguration::cDmConfigArgument,
                         Directory.WriteManifest(
                             "diagnostic_manager_manifest.arxml", _replacements)}};
            }
        };

        TEST_F(DiagnosticManagerTest, TerminateWithoutInitialization)
        {
            EXPECT_EQ(0, Manager->Terminate());
        }

        TEST_F(DiagnosticManagerTest, InitializationAndTermination)
        {
            PollingThread.Start();
            Manager->Initialize(GetArguments());

            EXPECT_TRUE(Stdout.WaitFor("Diagnostic Manager has been initialized."));
            EXPECT_TRUE(Stdout.WaitFor("TelematicControlModuleMonitor"));

            EXPECT_EQ(0, Manager->Terminate());
            EXPECT_TRUE(Stdout.Contains("Diagnostic Manager has been terminated."));
        }

        TEST_F(DiagnosticManagerTest, MissingNetworkEndpoint)
        {
            Manager->Initialize(
                GetArguments(
                    {{"<SHORT-NAME>DiagnosticManagerEP</SHORT-NAME>",
                      "<SHORT-NAME>UnknownEP</SHORT-NAME>"}}));

            EXPECT_EQ(1, Manager->Terminate());
            EXPECT_TRUE(Stdout.Contains("Fetching network configuration failed."));
            EXPECT_FALSE(Stdout.Contains("Diagnostic Manager has been initialized."));
        }

        TEST_F(DiagnosticManagerTest, MissingConfigArgument)
        {
            Manager->Initialize({});
            EXPECT_THROW(Manager->Terminate(), std::out_of_range);
        }

        TEST_F(DiagnosticManagerTest, MissingManifestFile)
        {
            Manager->Initialize(
                {{helper::ArgumentConfiguration::cDmConfigArgument,
                  Directory.GetFilePath("missing.arxml")}});
            EXPECT_THROW(Manager->Terminate(), std::invalid_argument);
        }
    }
}
