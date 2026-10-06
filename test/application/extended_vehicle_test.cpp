#include <gtest/gtest.h>
#include <atomic>
#include "../../src/ara/diag/conversation.h"
#include "../../src/application/doip/doip_client.h"
#include "../../src/application/extended_vehicle.h"
#include "../../src/application/helper/argument_configuration.h"
#include "../ara/phm/mocked_checkpoint_communicator.h"
#include "./helper/mock_http_server.h"
#include "./helper/testable_fixture.h"

namespace application
{
    class ExtendedVehicleTest : public testing::Test
    {
    protected:
        const std::string cVin{"YV1ABCDEFGH123456"};
        const uint16_t cDoipPort;

        fixture::StdoutCapture Stdout;
        fixture::TemporaryDirectory Directory;
        fixture::MockHttpServer HttpServer;
        AsyncBsdSocketLib::Poller Poller;
        fixture::PollingThread PollingThread;
        ara::phm::MockedCheckpointCommunicator Communicator;
        std::atomic<uint32_t> CheckpointCounts[3];
        std::unique_ptr<ExtendedVehicle> Vehicle;

        ExtendedVehicleTest() : cDoipPort{fixture::GetFreePort()},
                                PollingThread(&Poller),
                                CheckpointCounts{{0}, {0}, {0}}
        {
            Communicator.SetCallback(
                [this](uint32_t checkpoint)
                {
                    if (checkpoint < 3)
                    {
                        ++CheckpointCounts[checkpoint];
                    }
                });
        }

        void SetUp() override
        {
            Vehicle.reset(
                new ExtendedVehicle(&Poller, &Communicator, HttpServer.GetUrl("/vehicles")));
        }

        void TearDown() override
        {
            Vehicle->Terminate();
            PollingThread.Stop();
            Vehicle.reset();
        }

        std::map<std::string, std::string> GetArguments(
            const std::map<std::string, std::string> &replacements = {})
        {
            std::map<std::string, std::string> _replacements(replacements);
            _replacements["<PORT-NUMBER>8081</PORT-NUMBER>"] =
                "<PORT-NUMBER>" + std::to_string(cDoipPort) + "</PORT-NUMBER>";
            _replacements["<EVENT-MULTICAST-UDP-PORT>5555</EVENT-MULTICAST-UDP-PORT>"] =
                "<EVENT-MULTICAST-UDP-PORT>" +
                std::to_string(fixture::GetFreePort(SOCK_DGRAM)) +
                "</EVENT-MULTICAST-UDP-PORT>";

            return {
                {helper::ArgumentConfiguration::cEvConfigArgument,
                 Directory.WriteManifest("extended_vehicle_manifest.arxml", _replacements)},
                {helper::ArgumentConfiguration::cApiKeyArgument, "api-key"},
                {helper::ArgumentConfiguration::cBearerTokenArgument, "bearer-token"}};
        }
    };

    TEST_F(ExtendedVehicleTest, DefaultVehiclesUrl)
    {
        EXPECT_EQ(
            "https://api.volvocars.com/extended-vehicle/v1/vehicles",
            ExtendedVehicle::cDefaultVehiclesUrl);
    }

    TEST_F(ExtendedVehicleTest, TerminateWithoutInitialization)
    {
        EXPECT_EQ(0, Vehicle->Terminate());
    }

    TEST_F(ExtendedVehicleTest, ConfiguredVehicle)
    {
        HttpServer.SetResponse(
            "/vehicles", "{\"vehicles\":[{\"id\":\"" + cVin + "\"}]}");
        HttpServer.SetResponse(
            "/vehicles/" + cVin + "/resources/averageSpeed",
            "{\"averageSpeed\":{\"value\":\"65\"}}");

        const bool cNoActiveConversation{
            ara::diag::Conversation::GetCurrentActiveConversations().empty()};

        PollingThread.Start();
        Vehicle->Initialize(GetArguments());

        ASSERT_TRUE(Stdout.WaitFor("Extended Vehicle AA has been initialized."));
        ASSERT_TRUE(Stdout.WaitFor("The VIN is set to " + cVin));

        const std::string cRequest{HttpServer.GetLastRequest()};
        EXPECT_NE(std::string::npos, cRequest.find("vcc-api-key: api-key\r\n"));
        EXPECT_NE(std::string::npos, cRequest.find("Authorization: Bearer bearer-token\r\n"));

        // The configured DoIP server serves UDS requests from the REST resources
        std::mutex _mutex;
        std::vector<uint8_t> _udsResponse;
        doip::DoipClient _client(
            &Poller, "127.0.0.1", cDoipPort, 2,
            [&_mutex, &_udsResponse](std::vector<uint8_t> &&response)
            {
                std::lock_guard<std::mutex> _lock(_mutex);
                _udsResponse = std::move(response);
            });

        EXPECT_TRUE(
            fixture::WaitUntil(
                [&_client]()
                { return _client.TrySendDiagMessage({0x22, 0xf5, 0x0d}); }));
        EXPECT_TRUE(
            fixture::WaitUntil(
                [&_mutex, &_udsResponse]()
                {
                    std::lock_guard<std::mutex> _lock(_mutex);
                    return !_udsResponse.empty();
                }));
        PollingThread.Stop();

        const std::vector<uint8_t> cExpected{0x62, 0xf5, 0x0d, 65};
        EXPECT_EQ(cExpected, _udsResponse);

        EXPECT_EQ(0, Vehicle->Terminate());
        // Conversations are process-wide, so earlier tests in the same binary may leave active ones
        EXPECT_EQ(
            cNoActiveConversation,
            Stdout.Contains("There was no active diagnostic conversation at the termination."));
        EXPECT_TRUE(Stdout.Contains("Extended Vehicle AA has been terminated."));
    }

    TEST_F(ExtendedVehicleTest, SupervisionCheckpoints)
    {
        Vehicle->Initialize(GetArguments());
        ASSERT_TRUE(Stdout.WaitFor("Extended Vehicle AA has been initialized."));

        EXPECT_TRUE(
            fixture::WaitUntil(
                [this]()
                { return CheckpointCounts[2] > 1; }));
        EXPECT_EQ(0, Vehicle->Terminate());

        EXPECT_GT(CheckpointCounts[0], 1);
        EXPECT_GT(CheckpointCounts[1], 1);
        EXPECT_GT(CheckpointCounts[2], 1);
    }

    TEST_F(ExtendedVehicleTest, RestfulApiError)
    {
        HttpServer.SetResponse(
            "/vehicles", "{\"error\":{\"message\":\"Unauthorized request\"}}", 401);
        Vehicle->Initialize(GetArguments());

        EXPECT_TRUE(Stdout.WaitFor("Setting the VIN failed. Unauthorized request"));
        EXPECT_EQ(0, Vehicle->Terminate());
        EXPECT_FALSE(Stdout.Contains("The VIN is set to"));
    }

    TEST_F(ExtendedVehicleTest, UnexpectedRestfulResponse)
    {
        HttpServer.SetResponse("/vehicles", "{\"unexpected\":true}");
        Vehicle->Initialize(GetArguments());

        EXPECT_TRUE(
            Stdout.WaitFor("Setting the VIN failed due to unexpected RESTful response format."));
        EXPECT_EQ(0, Vehicle->Terminate());
    }

    TEST_F(ExtendedVehicleTest, UnreachableRestfulApi)
    {
        Vehicle.reset(
            new ExtendedVehicle(
                &Poller, &Communicator,
                "http://127.0.0.1:" + std::to_string(fixture::GetFreePort()) + "/vehicles"));
        Vehicle->Initialize(GetArguments());

        EXPECT_TRUE(Stdout.WaitFor("Extended Vehicle AA has been initialized."));
        EXPECT_TRUE(fixture::WaitUntil([this]()
                                       { return CheckpointCounts[2] > 0; }));
        EXPECT_EQ(0, Vehicle->Terminate());
        EXPECT_FALSE(Stdout.Contains("The VIN is set to"));
    }

    TEST_F(ExtendedVehicleTest, MissingNetworkEndpoint)
    {
        HttpServer.SetResponse(
            "/vehicles", "{\"vehicles\":[{\"id\":\"" + cVin + "\"}]}");
        Vehicle->Initialize(
            GetArguments(
                {{"<SHORT-NAME>ExtendedVehicleEP</SHORT-NAME>",
                  "<SHORT-NAME>UnknownEP</SHORT-NAME>"}}));

        EXPECT_EQ(1, Vehicle->Terminate());
        EXPECT_TRUE(Stdout.Contains("Fetching network configuration failed."));
    }

    TEST_F(ExtendedVehicleTest, MissingApiKeyArgument)
    {
        auto _arguments{GetArguments()};
        _arguments.erase(helper::ArgumentConfiguration::cApiKeyArgument);
        Vehicle->Initialize(_arguments);

        EXPECT_THROW(Vehicle->Terminate(), std::out_of_range);
    }

    TEST_F(ExtendedVehicleTest, MissingManifestFile)
    {
        Vehicle->Initialize(
            {{helper::ArgumentConfiguration::cEvConfigArgument,
              Directory.GetFilePath("missing.arxml")}});

        EXPECT_THROW(Vehicle->Terminate(), std::invalid_argument);
    }
}
