#include <gtest/gtest.h>
#include "../../../src/application/helper/argument_configuration.h"
#include "../../../src/application/platform/platform_health_management.h"
#include "../../ara/phm/mocked_checkpoint_communicator.h"
#include "../helper/testable_fixture.h"

namespace application
{
    namespace platform
    {
        class PlatformHealthManagementTest : public testing::Test
        {
        protected:
            static const uint32_t cAliveCheckpoint{0};
            static const uint32_t cDeadlineSourceCheckpoint{1};

            fixture::StdoutCapture Stdout;
            fixture::TemporaryDirectory Directory;
            AsyncBsdSocketLib::Poller Poller;
            ara::phm::MockedCheckpointCommunicator Communicator;
            std::unique_ptr<PlatformHealthManagement> HealthManagement;

            void SetUp() override
            {
                HealthManagement.reset(
                    new PlatformHealthManagement(&Poller, &Communicator, "MachineFG"));
            }

            void TearDown() override
            {
                HealthManagement->Terminate();
                HealthManagement.reset();
            }

            std::map<std::string, std::string> GetArguments(
                const std::map<std::string, std::string> &replacements = {})
            {
                return {{helper::ArgumentConfiguration::cPhmConfigArgument,
                         Directory.WriteManifest(
                             "health_monitoring_manifest.arxml", replacements)}};
            }
        };

        const uint32_t PlatformHealthManagementTest::cAliveCheckpoint;
        const uint32_t PlatformHealthManagementTest::cDeadlineSourceCheckpoint;

        TEST_F(PlatformHealthManagementTest, TerminateWithoutInitialization)
        {
            EXPECT_EQ(0, HealthManagement->Terminate());
        }

        TEST_F(PlatformHealthManagementTest, CommunicatorCallbackIsRegistered)
        {
            EXPECT_TRUE(Communicator.TrySend(cAliveCheckpoint));
        }

        TEST_F(PlatformHealthManagementTest, InitializationAndTermination)
        {
            HealthManagement->Initialize(GetArguments());
            EXPECT_TRUE(Stdout.WaitFor("Plafrom health management has been initialized."));

            EXPECT_TRUE(Communicator.TrySend(cAliveCheckpoint));

            EXPECT_EQ(0, HealthManagement->Terminate());
            EXPECT_TRUE(Stdout.Contains("Plafrom health management has been terminated."));
        }

        TEST_F(PlatformHealthManagementTest, DeadlineSupervisionExpiration)
        {
            HealthManagement->Initialize(GetArguments());
            ASSERT_TRUE(Stdout.WaitFor("Plafrom health management has been initialized."));

            // Report the deadline source without the target checkpoint
            EXPECT_TRUE(Communicator.TrySend(cDeadlineSourceCheckpoint));
            EXPECT_TRUE(Stdout.WaitFor("Deadline supervision is expired on MachineFG"));
            EXPECT_EQ(0, HealthManagement->Terminate());
        }

        TEST_F(PlatformHealthManagementTest, AliveSupervisionExpiration)
        {
            HealthManagement->Initialize(
                GetArguments(
                    {{"<FAILED-REFERENCE-CYCLES-TOLERANCE>100</FAILED-REFERENCE-CYCLES-TOLERANCE>",
                      "<FAILED-REFERENCE-CYCLES-TOLERANCE>1</FAILED-REFERENCE-CYCLES-TOLERANCE>"}}));
            ASSERT_TRUE(Stdout.WaitFor("Plafrom health management has been initialized."));

            // No alive indication is reported, so the alive supervision expires
            EXPECT_TRUE(Stdout.WaitFor("Alive supervision is expired on MachineFG"));
            EXPECT_EQ(0, HealthManagement->Terminate());
        }

        TEST_F(PlatformHealthManagementTest, InvalidCheckpointReference)
        {
            HealthManagement->Initialize(
                GetArguments(
                    {{"<CHECKPOINT-REF DEST=\"SUPERVISION-CHECKPOINT\">0</CHECKPOINT-REF>",
                      "<CHECKPOINT-REF DEST=\"SUPERVISION-CHECKPOINT\">7</CHECKPOINT-REF>"}}));
            EXPECT_EQ(1, HealthManagement->Terminate());
            EXPECT_FALSE(Stdout.Contains("Plafrom health management has been initialized."));
        }

        TEST_F(PlatformHealthManagementTest, MissingManifestFile)
        {
            HealthManagement->Initialize(
                {{helper::ArgumentConfiguration::cPhmConfigArgument,
                  Directory.GetFilePath("missing.arxml")}});
            EXPECT_THROW(HealthManagement->Terminate(), std::invalid_argument);
        }
    }
}
