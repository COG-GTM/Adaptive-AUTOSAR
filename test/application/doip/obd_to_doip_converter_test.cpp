#include <gtest/gtest.h>
#include "../../../src/application/doip/obd_to_doip_converter.h"
#include "../../../src/application/doip/doip_server.h"
#include "./doip_test_helper.h"

namespace application
{
    namespace doip
    {
        struct ObdResponse
        {
            std::vector<uint8_t> Pid;
            std::vector<uint8_t> Data;
            uint8_t Service;
        };

        class ObdToDoipConverterFixture
        {
        protected:
            static const uint8_t cProtocolVersion{0x02};
            static const uint16_t cLogicalAddress{0x0e80};
            const std::string cIpAddress{"127.0.0.1"};
            const uint16_t cPort;

            AsyncBsdSocketLib::Poller Poller;
            StubHttpServer HttpServer;
            helper::CurlWrapper Curl;
            std::unique_ptr<DoipServer> Server;
            std::unique_ptr<ObdToDoipConverter> Converter;
            std::vector<ObdResponse> Responses;

            ObdToDoipConverterFixture() : cPort{GetFreePort()},
                                          HttpServer(
                                              "{\"averageSpeed\":{\"value\":\"50\"},"
                                              "\"fuelAmount\":{\"value\":\"40\"},"
                                              "\"externalTemp\":{\"value\":\"20\"},"
                                              "\"averageFuelConsumption\":{\"value\":\"6.4\"},"
                                              "\"engineCoolantTemp\":{\"value\":\"90\"},"
                                              "\"odometer\":{\"value\":\"1000.5\"}}"),
                                          Curl("api-key", "token")
            {
                std::string _vin{"WAUZZZ8V9KA000001"};
                Server.reset(
                    new DoipServer(
                        &Poller,
                        &Curl,
                        HttpServer.GetUrl(),
                        cIpAddress,
                        cPort,
                        CreateControllerConfig(cProtocolVersion),
                        std::move(_vin),
                        cLogicalAddress,
                        0x01,
                        0x02));

                Converter.reset(
                    new ObdToDoipConverter(&Poller, cIpAddress, cPort));

                ObdEmulator::ObdService::CallbackType _callback{
                    [this](
                        const std::vector<uint8_t> &pid,
                        std::vector<uint8_t> &&data,
                        uint8_t service)
                    {
                        ObdResponse _response;
                        _response.Pid = pid;
                        _response.Data = std::move(data);
                        _response.Service = service;
                        Responses.push_back(std::move(_response));
                    }};
                Converter->SetCallback(std::move(_callback));
            }

            /// @brief Poll till the converter can forward the PID query via DoIP
            bool TryQueryViaDoip(uint8_t pid)
            {
                const std::function<bool()> cCondition{
                    [this, pid]()
                    {
                        return Converter->TryGetResponseAsync({pid});
                    }};

                return PollUntil(Poller, cCondition);
            }

            bool WaitForResponses(size_t count)
            {
                const std::function<bool()> cCondition{
                    [this, count]()
                    {
                        return Responses.size() >= count;
                    }};

                return PollUntil(Poller, cCondition);
            }
        };

        const uint8_t ObdToDoipConverterFixture::cProtocolVersion;
        const uint16_t ObdToDoipConverterFixture::cLogicalAddress;

        class ObdToDoipConverterTest : public ObdToDoipConverterFixture,
                                       public testing::Test
        {
        };

        TEST_F(ObdToDoipConverterTest, ConstructionFailsWithoutServer)
        {
            const uint16_t cClosedPort{GetFreePort()};
            EXPECT_THROW(
                ObdToDoipConverter _converter(&Poller, cIpAddress, cClosedPort),
                std::runtime_error);
        }

        TEST_F(ObdToDoipConverterTest, ServiceIsShowCurrentData)
        {
            const uint8_t cExpectedResult{0x01};
            EXPECT_EQ(cExpectedResult, Converter->GetService());
        }

        TEST_F(ObdToDoipConverterTest, SynchronousResponseIsNotSupported)
        {
            const std::vector<uint8_t> cPid{0x0c};
            std::vector<uint8_t> _response;

            EXPECT_FALSE(Converter->TryGetResponse(cPid, _response));
            EXPECT_TRUE(_response.empty());
            EXPECT_TRUE(Responses.empty());
        }

        TEST_F(ObdToDoipConverterTest, UnsupportedPid)
        {
            // Monitor status since DTCs cleared (PID 0x01) is not emulated.
            EXPECT_FALSE(Converter->TryGetResponseAsync({0x01}));
            EXPECT_FALSE(Converter->TryGetResponseAsync({0xff}));
            EXPECT_TRUE(Responses.empty());
        }

        TEST_F(ObdToDoipConverterTest, EmptyPidThrows)
        {
            const std::vector<uint8_t> cPid;
            EXPECT_THROW(Converter->TryGetResponseAsync(cPid), std::out_of_range);
        }

        TEST_F(ObdToDoipConverterTest, OnlyFirstPidIsQueried)
        {
            const std::vector<uint8_t> cPid{0x0c, 0x01};
            const std::vector<uint8_t> cExpectedData{0x20, 0x00};

            ASSERT_TRUE(Converter->TryGetResponseAsync(cPid));

            ASSERT_EQ(1u, Responses.size());
            EXPECT_EQ(cPid, Responses.at(0).Pid);
            EXPECT_EQ(cExpectedData, Responses.at(0).Data);
        }

        TEST_F(ObdToDoipConverterTest, DoipPidBeforeVehicleIdentification)
        {
            // The vehicle speed (PID 0x0d) requires the DoIP logical address which is not fetched yet.
            EXPECT_FALSE(Converter->TryGetResponseAsync({0x0d}));
            EXPECT_TRUE(Responses.empty());
        }

        TEST_F(ObdToDoipConverterTest, NegativeUdsResponseIsNotCalledBack)
        {
            // Without the RESTful server the UDS server replies 'conditions not correct'.
            Converter.reset();
            Server.reset();
            Responses.clear();

            const std::string cUnreachableUrl{
                "http://127.0.0.1:" + std::to_string(GetFreePort())};
            std::string _vin{"WAUZZZ8V9KA000001"};
            Server.reset(
                new DoipServer(
                    &Poller,
                    &Curl,
                    cUnreachableUrl,
                    cIpAddress,
                    cPort,
                    CreateControllerConfig(cProtocolVersion),
                    std::move(_vin),
                    cLogicalAddress,
                    0x01,
                    0x02));
            Converter.reset(new ObdToDoipConverter(&Poller, cIpAddress, cPort));
            bool _called{false};
            ObdEmulator::ObdService::CallbackType _callback{
                [&_called](
                    const std::vector<uint8_t> &pid,
                    std::vector<uint8_t> &&data,
                    uint8_t service)
                {
                    _called = true;
                }};
            Converter->SetCallback(std::move(_callback));

            ASSERT_TRUE(TryQueryViaDoip(0x0d));
            const std::chrono::milliseconds cSettleTime{300};
            PollFor(Poller, cSettleTime);

            EXPECT_FALSE(_called);
        }

        /// @brief A PID which is supported based on the emulated 'supported PIDs' bitmaps should be answerable
        TEST_F(ObdToDoipConverterTest, SupportedPidBitmapsMatchAnswerablePids)
        {
            const size_t cBitmapSize{4};
            const uint8_t cRangeSize{0x20};
            const std::vector<uint8_t> cBitmapPids{
                0x00, 0x20, 0x40, 0x60, 0x80, 0xa0};

            // Fetch the logical address first to make the DoIP PIDs answerable
            ASSERT_TRUE(TryQueryViaDoip(0x0d));
            ASSERT_TRUE(WaitForResponses(1));

            for (const uint8_t cBitmapPid : cBitmapPids)
            {
                Responses.clear();
                ASSERT_TRUE(Converter->TryGetResponseAsync({cBitmapPid}));
                ASSERT_EQ(1u, Responses.size());
                const std::vector<uint8_t> cBitmap{Responses.at(0).Data};
                ASSERT_EQ(cBitmapSize, cBitmap.size());

                for (uint8_t i = 0; i < cRangeSize; ++i)
                {
                    const uint8_t cPid{static_cast<uint8_t>(cBitmapPid + i + 1)};
                    const uint8_t cOctet{cBitmap.at(i / 8)};
                    const bool cSupported{((cOctet >> (7 - (i % 8))) & 0x01) == 0x01};

                    EXPECT_EQ(cSupported, Converter->TryGetResponseAsync({cPid}))
                        << "PID 0x" << std::hex << static_cast<int>(cPid);
                }
            }
        }

        struct StaticPidCase
        {
            uint8_t Pid;
            std::vector<uint8_t> Data;
        };

        class ObdToDoipConverterStaticPidTest : public ObdToDoipConverterFixture,
                                                public testing::TestWithParam<StaticPidCase>
        {
        };

        TEST_P(ObdToDoipConverterStaticPidTest, EmulatedValue)
        {
            const StaticPidCase cCase{GetParam()};
            const std::vector<uint8_t> cPid{cCase.Pid};
            const uint8_t cExpectedService{0x01};

            EXPECT_TRUE(Converter->TryGetResponseAsync(cPid));

            ASSERT_EQ(1u, Responses.size());
            EXPECT_EQ(cPid, Responses.at(0).Pid);
            EXPECT_EQ(cCase.Data, Responses.at(0).Data);
            EXPECT_EQ(cExpectedService, Responses.at(0).Service);
            // The emulated values are served locally without any RESTful request.
            EXPECT_EQ(0, HttpServer.GetRequestCount());
        }

        INSTANTIATE_TEST_SUITE_P(
            EmulatedPids,
            ObdToDoipConverterStaticPidTest,
            testing::Values(
                StaticPidCase{0x00, {0x3e, 0x7f, 0xf0, 0x15}},
                StaticPidCase{0x03, {0x02, 0x00}},
                StaticPidCase{0x04, {0x33}},
                StaticPidCase{0x06, {0x8c}},
                StaticPidCase{0x07, {0x80}},
                StaticPidCase{0x0a, {0x50}},
                StaticPidCase{0x0b, {0x4b}},
                StaticPidCase{0x0c, {0x20, 0x00}},
                StaticPidCase{0x0e, {0x92}},
                StaticPidCase{0x0f, {0x50}},
                StaticPidCase{0x10, {0x02, 0x08}},
                StaticPidCase{0x11, {0x33}},
                StaticPidCase{0x12, {0x04}},
                StaticPidCase{0x13, {0x01}},
                StaticPidCase{0x14, {0xc8, 0xff}},
                StaticPidCase{0x1c, {0x01}},
                StaticPidCase{0x1e, {0x00}},
                StaticPidCase{0x20, {0xd0, 0x02, 0x70, 0x11}},
                StaticPidCase{0x21, {0x00, 0x00}},
                StaticPidCase{0x22, {0x19, 0x78}},
                StaticPidCase{0x24, {0x80, 0x00, 0x20, 0x00}},
                StaticPidCase{0x32, {0x6a, 0x40}},
                StaticPidCase{0x33, {0x65}},
                StaticPidCase{0x34, {0x80, 0x00, 0xe4, 0x00}},
                StaticPidCase{0x3c, {0x19, 0x00}},
                StaticPidCase{0x40, {0x5c, 0x0b, 0xb0, 0xd5}},
                StaticPidCase{0x42, {0x2e, 0xe0}},
                StaticPidCase{0x44, {0x80, 0x00}},
                StaticPidCase{0x45, {0x33}},
                StaticPidCase{0x4d, {0x00, 0x00}},
                StaticPidCase{0x4f, {0x02, 0x08, 0x80, 0x19}},
                StaticPidCase{0x50, {0x41, 0x00, 0x00, 0x00}},
                StaticPidCase{0x51, {0x01}},
                StaticPidCase{0x53, {0x54, 0x38}},
                StaticPidCase{0x54, {0x1a, 0x90}},
                StaticPidCase{0x59, {0x4e, 0x20}},
                StaticPidCase{0x5a, {0x33}},
                StaticPidCase{0x5c, {0xd2}},
                StaticPidCase{0x60, {0xe3, 0x00, 0x00, 0x01}},
                StaticPidCase{0x61, {0x91}},
                StaticPidCase{0x62, {0x91}},
                StaticPidCase{0x63, {0x01, 0x00}},
                StaticPidCase{0x67, {0x01, 0x6e, 0x00}},
                StaticPidCase{0x68, {0x01, 0x50, 0x00}},
                StaticPidCase{0x80, {0x00, 0x00, 0x00, 0x01}},
                StaticPidCase{0xa0, {0x04, 0x00, 0x00, 0x00}}));

        class ObdToDoipConverterDoipPidTest : public ObdToDoipConverterFixture,
                                              public testing::TestWithParam<StaticPidCase>
        {
        };

        /// @note End-to-end over loopback: OBD query -> DoIP client -> DoIP server -> UDS router ->
        ///       ReadDataByIdentifier -> libcurl -> HTTP stub (stand-in for the vehicle RESTful API)
        TEST_P(ObdToDoipConverterDoipPidTest, ValueFetchedViaDoip)
        {
            const StaticPidCase cCase{GetParam()};
            const std::vector<uint8_t> cPid{cCase.Pid};
            const uint8_t cExpectedService{0x01};

            ASSERT_TRUE(TryQueryViaDoip(cCase.Pid));
            ASSERT_TRUE(WaitForResponses(1));

            EXPECT_EQ(cPid, Responses.at(0).Pid);
            EXPECT_EQ(cCase.Data, Responses.at(0).Data);
            EXPECT_EQ(cExpectedService, Responses.at(0).Service);
            EXPECT_EQ(1, HttpServer.GetRequestCount());
        }

        INSTANTIATE_TEST_SUITE_P(
            DoipPids,
            ObdToDoipConverterDoipPidTest,
            testing::Values(
                // Engine coolant temperature: 90 °C + 40
                StaticPidCase{0x05, {130}},
                // Vehicle speed: 50 km/h
                StaticPidCase{0x0d, {50}},
                // Fuel tank level: 40% * 2.55
                StaticPidCase{0x2f, {102}},
                // Ambient air temperature: 20 °C + 40
                StaticPidCase{0x46, {60}},
                // Engine fuel rate: 6.4 L/h * 20
                StaticPidCase{0x5e, {0x00, 0x80}},
                // Odometer: 1000.5 km * 10
                StaticPidCase{0xa6, {0x00, 0x00, 0x27, 0x15}}));
    }
}
