#include <gtest/gtest.h>
#include "../../../src/ara/com/someip/rpc/socket_rpc_server.h"
#include "../../../src/ara/exec/execution_server.h"
#include "../../../src/ara/exec/state_server.h"
#include "../../../src/application/helper/argument_configuration.h"
#include "../../../src/application/platform/state_management.h"
#include "../helper/testable_fixture.h"

namespace application
{
    namespace platform
    {
        class StateManagementTest : public testing::Test
        {
        protected:
            fixture::StdoutCapture Stdout;
            fixture::TemporaryDirectory Directory;
            const uint16_t cPort;
            AsyncBsdSocketLib::Poller Poller;
            fixture::PollingThread PollingThread;
            std::unique_ptr<ara::com::someip::rpc::SocketRpcServer> RpcServer;
            std::unique_ptr<ara::exec::ExecutionServer> ExecutionServer;
            std::unique_ptr<ara::exec::StateServer> StateServer;
            std::unique_ptr<StateManagement> Management;

            StateManagementTest() : cPort{fixture::GetFreePort()},
                                    PollingThread(&Poller)
            {
            }

            std::map<std::string, std::string> GetArguments(
                const std::map<std::string, std::string> &replacements = {})
            {
                std::map<std::string, std::string> _replacements(replacements);
                _replacements["<PORT-NUMBER>8080</PORT-NUMBER>"] =
                    "<PORT-NUMBER>" + std::to_string(cPort) + "</PORT-NUMBER>";

                return {{helper::ArgumentConfiguration::cConfigArgument,
                         Directory.WriteManifest("execution_manifest.arxml", _replacements)}};
            }

            void StartServers()
            {
                RpcServer.reset(
                    new ara::com::someip::rpc::SocketRpcServer(
                        &Poller, "127.0.0.1", cPort, 1));
                ExecutionServer.reset(new ara::exec::ExecutionServer(RpcServer.get()));
                StateServer.reset(
                    new ara::exec::StateServer(
                        RpcServer.get(),
                        {{"MachineFG", "Off"}, {"MachineFG", "StartUp"}},
                        {{"MachineFG", "Off"}}));
            }

            void SetUp() override
            {
                Management.reset(new StateManagement(&Poller));
            }

            void TearDown() override
            {
                Management->Terminate();
                PollingThread.Stop();
                Management.reset();
                StateServer.reset();
                ExecutionServer.reset();
                RpcServer.reset();
            }
        };

        TEST_F(StateManagementTest, TerminateWithoutInitialization)
        {
            EXPECT_EQ(0, Management->Terminate());
        }

        TEST_F(StateManagementTest, StartUpTransition)
        {
            StartServers();
            PollingThread.Start();
            Management->Initialize(GetArguments());

            EXPECT_TRUE(Stdout.WaitFor("Function group: MachineFG is configured."));
            EXPECT_TRUE(Stdout.WaitFor("State: Off of function group: MachineFG is configured."));
            EXPECT_TRUE(Stdout.WaitFor("State: StartUp of function group: MachineFG is configured."));
            EXPECT_TRUE(Stdout.WaitFor("State management has been initialized."));
            EXPECT_TRUE(Stdout.WaitFor("Execution state is reported successfully."));
            EXPECT_TRUE(Stdout.WaitFor("EM is transited to the start-up state successfully."));

            std::string _state;
            EXPECT_TRUE(StateServer->TryGetState("MachineFG", _state));
            EXPECT_EQ("StartUp", _state);

            ara::exec::ExecutionState _executionState;
            EXPECT_TRUE(ExecutionServer->TryGetExecutionState("StateManagement", _executionState));
            EXPECT_EQ(ara::exec::ExecutionState::kRunning, _executionState);

            EXPECT_EQ(0, Management->Terminate());
            EXPECT_TRUE(Stdout.Contains("State management has been terminated."));
        }

        TEST_F(StateManagementTest, MissingStartUpState)
        {
            StartServers();
            PollingThread.Start();
            Management->Initialize(
                GetArguments(
                    {{"<SHORT-NAME>StartUp</SHORT-NAME>", "<SHORT-NAME>Running</SHORT-NAME>"}}));

            EXPECT_TRUE(
                Stdout.WaitFor("State: StartUp for function group MachineFG cannot be found."));
            EXPECT_EQ(1, Management->Terminate());
        }

        TEST_F(StateManagementTest, MissingRpcServer)
        {
            Management->Initialize(GetArguments());
            EXPECT_EQ(1, Management->Terminate());
        }

        TEST_F(StateManagementTest, MissingRpcConfiguration)
        {
            Management->Initialize(
                GetArguments(
                    {{"<PROTOCOL-VERSION>1</PROTOCOL-VERSION>", ""}}));

            EXPECT_TRUE(Stdout.WaitFor("RPC configuration failed."));
            EXPECT_EQ(1, Management->Terminate());
        }

        TEST_F(StateManagementTest, MissingConfigArgument)
        {
            Management->Initialize({});
            EXPECT_THROW(Management->Terminate(), std::out_of_range);
        }

        TEST_F(StateManagementTest, MissingManifestFile)
        {
            Management->Initialize(
                {{helper::ArgumentConfiguration::cConfigArgument,
                  Directory.GetFilePath("missing.arxml")}});
            EXPECT_THROW(Management->Terminate(), std::invalid_argument);
        }
    }
}
