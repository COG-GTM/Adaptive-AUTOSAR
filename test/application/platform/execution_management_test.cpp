#include <gtest/gtest.h>
#include <sys/stat.h>
#include "../../../src/application/helper/argument_configuration.h"
#include "../../../src/application/platform/execution_management.h"
#include "../helper/mock_http_server.h"
#include "../helper/testable_fixture.h"

namespace application
{
    namespace platform
    {
        class ExecutionManagementTest : public testing::Test
        {
        protected:
            const std::string cVin{"YV1ABCDEFGH123456"};

            fixture::StdoutCapture Stdout;
            fixture::TemporaryDirectory Directory;
            fixture::MockHttpServer HttpServer;
            AsyncBsdSocketLib::Poller Poller;
            fixture::PollingThread PollingThread;
            std::unique_ptr<ExecutionManagement> Management;

            ExecutionManagementTest() : PollingThread(&Poller)
            {
                HttpServer.SetResponse(
                    "/vehicles",
                    "{\"vehicles\":[{\"id\":\"" + cVin + "\"}]}");
            }

            void SetUp() override
            {
                Management.reset(
                    new ExecutionManagement(&Poller, HttpServer.GetUrl("/vehicles")));
            }

            void TearDown() override
            {
                Management->Terminate();
                PollingThread.Stop();
                Management.reset();
            }

            static std::string toPort(uint16_t port, const std::string &tag)
            {
                return "<" + tag + ">" + std::to_string(port) + "</" + tag + ">";
            }

            std::map<std::string, std::string> GetArguments(
                const std::map<std::string, std::string> &emReplacements = {})
            {
                // The DM and EV use different SD ports, so the DM does not discover the EV.
                std::map<std::string, std::string> _emReplacements(emReplacements);
                _emReplacements["<PORT-NUMBER>8080</PORT-NUMBER>"] =
                    toPort(fixture::GetFreePort(), "PORT-NUMBER");

                return {
                    {helper::ArgumentConfiguration::cConfigArgument,
                     Directory.WriteManifest("execution_manifest.arxml", _emReplacements)},
                    {helper::ArgumentConfiguration::cEvConfigArgument,
                     Directory.WriteManifest(
                         "extended_vehicle_manifest.arxml",
                         {{"<PORT-NUMBER>8081</PORT-NUMBER>",
                           toPort(fixture::GetFreePort(), "PORT-NUMBER")},
                          {"<EVENT-MULTICAST-UDP-PORT>5555</EVENT-MULTICAST-UDP-PORT>",
                           toPort(fixture::GetFreePort(SOCK_DGRAM), "EVENT-MULTICAST-UDP-PORT")}})},
                    {helper::ArgumentConfiguration::cDmConfigArgument,
                     Directory.WriteManifest(
                         "diagnostic_manager_manifest.arxml",
                         {{"<PORT-NUMBER>5555</PORT-NUMBER>",
                           toPort(fixture::GetFreePort(SOCK_DGRAM), "PORT-NUMBER")}})},
                    {helper::ArgumentConfiguration::cPhmConfigArgument,
                     fixture::GetConfigurationPath("health_monitoring_manifest.arxml")},
                    {helper::ArgumentConfiguration::cApiKeyArgument, "api-key"},
                    {helper::ArgumentConfiguration::cBearerTokenArgument, "bearer-token"}};
            }
        };

        TEST_F(ExecutionManagementTest, CheckpointFifoIsCreated)
        {
            struct stat _status;
            ASSERT_EQ(0, stat("/tmp/fifo_communicator", &_status));
            EXPECT_TRUE(S_ISFIFO(_status.st_mode));
        }

        TEST_F(ExecutionManagementTest, TerminateWithoutInitialization)
        {
            EXPECT_EQ(0, Management->Terminate());
        }

        TEST_F(ExecutionManagementTest, StartUpAndShutdown)
        {
            PollingThread.Start();
            Management->Initialize(GetArguments());

            EXPECT_TRUE(Stdout.WaitFor("Execution management has been initialized."));
            EXPECT_TRUE(Stdout.WaitFor("State management has been initialized."));
            EXPECT_TRUE(Stdout.WaitFor("Execution state is reported successfully."));
            EXPECT_TRUE(Stdout.WaitFor("EM is transited to the start-up state successfully."));
            EXPECT_TRUE(Stdout.WaitFor("Plafrom health management has been initialized."));
            EXPECT_TRUE(Stdout.WaitFor("Diagnostic Manager has been initialized."));
            EXPECT_TRUE(Stdout.WaitFor("Extended Vehicle AA has been initialized."));
            EXPECT_TRUE(Stdout.WaitFor("The VIN is set to " + cVin));

            EXPECT_EQ(0, Management->Terminate());
            EXPECT_TRUE(Stdout.Contains("Extended Vehicle AA has been terminated."));
            EXPECT_TRUE(Stdout.Contains("Diagnostic Manager has been terminated."));
            EXPECT_TRUE(Stdout.Contains("Plafrom health management has been terminated."));
            EXPECT_TRUE(Stdout.Contains("State management has been terminated."));
            EXPECT_TRUE(Stdout.Contains("Execution management has been terminated."));
        }

        TEST_F(ExecutionManagementTest, MissingRpcEndpoint)
        {
            Management->Initialize(
                GetArguments(
                    {{"<SHORT-NAME>RpcServerEP</SHORT-NAME>",
                      "<SHORT-NAME>UnknownEP</SHORT-NAME>"}}));

            EXPECT_EQ(1, Management->Terminate());
            EXPECT_TRUE(Stdout.Contains("RPC configuration failed."));
            EXPECT_FALSE(Stdout.Contains("Execution management has been initialized."));
        }

        TEST_F(ExecutionManagementTest, MissingManifestFile)
        {
            auto _arguments{GetArguments()};
            _arguments[helper::ArgumentConfiguration::cConfigArgument] =
                Directory.GetFilePath("missing.arxml");
            Management->Initialize(_arguments);

            EXPECT_THROW(Management->Terminate(), std::invalid_argument);
        }
    }
}
