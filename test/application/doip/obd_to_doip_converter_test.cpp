#include <gtest/gtest.h>
#include <mutex>
#include "../../../src/application/doip/doip_server.h"
#include "../../../src/application/doip/obd_to_doip_converter.h"
#include "../helper/mock_http_server.h"
#include "../helper/testable_fixture.h"

namespace application
{
    namespace doip
    {
        class ObdToDoipConverterTest : public testing::Test
        {
        protected:
            struct ObdResponse
            {
                std::vector<uint8_t> pid;
                std::vector<uint8_t> data;
                uint8_t service;
            };

            const std::string cIpAddress{"127.0.0.1"};
            const uint16_t cPort;

            fixture::MockHttpServer HttpServer;
            helper::CurlWrapper Curl;
            AsyncBsdSocketLib::Poller Poller;
            fixture::PollingThread PollingThread;
            std::unique_ptr<DoipServer> Server;
            std::unique_ptr<ObdToDoipConverter> Converter;

            std::mutex Mutex;
            std::vector<ObdResponse> Responses;

            ObdToDoipConverterTest() : cPort{fixture::GetFreePort()},
                                       Curl("api-key", "bearer-token"),
                                       PollingThread(&Poller)
            {
                HttpServer.SetResponse(
                    "/resources/averageSpeed",
                    "{\"averageSpeed\":{\"value\":\"65\"}}");
            }

            void SetUp() override
            {
                DoipLib::ControllerConfig _config;
                _config.protocolVersion = 2;
                _config.doipMaxRequestBytes = DoipServer::cDoipPacketSize;
                _config.doIPInitialVehicleAnnouncementTime = std::chrono::seconds(1);
                _config.doIPVehicleAnnouncementCount = 3;
                _config.doIPVehicleAnnouncementInterval = std::chrono::seconds(1);

                Server.reset(
                    new DoipServer(
                        &Poller, &Curl, HttpServer.GetUrl("/resources"),
                        cIpAddress, cPort, std::move(_config),
                        "YV1ABCDEFGH123456", 1, 1, 1));

                Converter.reset(new ObdToDoipConverter(&Poller, cIpAddress, cPort));
                Converter->SetCallback(
                    [this](const std::vector<uint8_t> &pid,
                           std::vector<uint8_t> &&data,
                           uint8_t service)
                    {
                        std::lock_guard<std::mutex> _lock(Mutex);
                        Responses.push_back({pid, std::move(data), service});
                    });
            }

            void TearDown() override
            {
                PollingThread.Stop();
                Converter.reset();
                Server.reset();
            }

            size_t GetResponseCount()
            {
                std::lock_guard<std::mutex> _lock(Mutex);
                return Responses.size();
            }
        };

        TEST_F(ObdToDoipConverterTest, Service)
        {
            EXPECT_EQ(0x01, Converter->GetService());
        }

        TEST_F(ObdToDoipConverterTest, SynchronousResponseIsNotSupported)
        {
            std::vector<uint8_t> _response;
            EXPECT_FALSE(Converter->TryGetResponse({0x0c}, _response));
            EXPECT_TRUE(_response.empty());
        }

        TEST_F(ObdToDoipConverterTest, LocalPid)
        {
            EXPECT_TRUE(Converter->TryGetResponseAsync({0x0c}));

            ASSERT_EQ(1, GetResponseCount());
            const std::vector<uint8_t> cExpectedPid{0x0c};
            const std::vector<uint8_t> cExpectedData{0x20, 0x00};
            EXPECT_EQ(cExpectedPid, Responses.front().pid);
            EXPECT_EQ(cExpectedData, Responses.front().data);
            EXPECT_EQ(0x01, Responses.front().service);
        }

        TEST_F(ObdToDoipConverterTest, SupportedPidsBitmap)
        {
            EXPECT_TRUE(Converter->TryGetResponseAsync({0x00}));

            ASSERT_EQ(1, GetResponseCount());
            const std::vector<uint8_t> cExpectedData{0x3e, 0x7f, 0xf0, 0x15};
            EXPECT_EQ(cExpectedData, Responses.front().data);
        }

        TEST_F(ObdToDoipConverterTest, UnsupportedPid)
        {
            EXPECT_FALSE(Converter->TryGetResponseAsync({0xff}));
            EXPECT_EQ(0, GetResponseCount());
        }

        TEST_F(ObdToDoipConverterTest, EmptyPid)
        {
            EXPECT_THROW(Converter->TryGetResponseAsync({}), std::out_of_range);
        }

        TEST_F(ObdToDoipConverterTest, RemotePidBeforeVehicleIdentification)
        {
            EXPECT_FALSE(Converter->TryGetResponseAsync({0x0d}));
        }

        TEST_F(ObdToDoipConverterTest, RemotePid)
        {
            PollingThread.Start();
            ASSERT_TRUE(
                fixture::WaitUntil(
                    [this]()
                    { return Converter->TryGetResponseAsync({0x0d}); }));
            ASSERT_TRUE(
                fixture::WaitUntil(
                    [this]()
                    { return GetResponseCount() > 0; }));
            PollingThread.Stop();

            std::lock_guard<std::mutex> _lock(Mutex);
            const std::vector<uint8_t> cExpectedPid{0x0d};
            const std::vector<uint8_t> cExpectedData{65};
            EXPECT_EQ(cExpectedPid, Responses.front().pid);
            EXPECT_EQ(cExpectedData, Responses.front().data);
        }

        TEST_F(ObdToDoipConverterTest, NegativeRemoteResponseIsDropped)
        {
            HttpServer.SetResponse("/resources/fuelAmount", "invalid");
            PollingThread.Start();
            ASSERT_TRUE(
                fixture::WaitUntil(
                    [this]()
                    { return Converter->TryGetResponseAsync({0x2f}); }));
            ASSERT_TRUE(
                fixture::WaitUntil(
                    [this]()
                    { return HttpServer.GetRequestCount() > 0; }));
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            PollingThread.Stop();

            EXPECT_EQ(0, GetResponseCount());
        }
    }
}
