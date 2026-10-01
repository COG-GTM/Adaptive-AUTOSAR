#include <gtest/gtest.h>
#include "../../../src/application/doip/obd_to_doip_converter.h"
#include "./mock_doip_server.h"

namespace application
{
    namespace doip
    {
        class ObdToDoipConverterTest : public testing::Test
        {
        private:
            static const int cPollTimeout{10};
            static const int cMaxPollIterations{200};

            std::unique_ptr<ObdToDoipConverter> mConverter;

        protected:
            static const uint8_t cService{0x01};
            static const uint8_t cProtocolVersion{0x02};
            static const uint16_t cLogicalAddress{0x0e80};
            const std::string cIpAddress{"127.0.0.1"};
            const std::string cVin{"WVWZZZ1JZXW000001"};

            AsyncBsdSocketLib::Poller Poller;
            MockDoipServer Server;

            bool CallbackInvoked{false};
            std::vector<uint8_t> ReceivedPid;
            std::vector<uint8_t> ReceivedData;
            uint8_t ReceivedService{0};

            ObdToDoipConverterTest() : Server(&Poller, cIpAddress, cProtocolVersion, cVin, cLogicalAddress)
            {
            }

            void SetUp() override
            {
                mConverter.reset(
                    new ObdToDoipConverter(&Poller, cIpAddress, Server.Port()));

                mConverter->SetCallback(
                    [this](const std::vector<uint8_t> &pid, std::vector<uint8_t> &&data, uint8_t service)
                    {
                        CallbackInvoked = true;
                        ReceivedPid = pid;
                        ReceivedData = std::move(data);
                        ReceivedService = service;
                    });
            }

            void TearDown() override
            {
                mConverter.reset();
            }

            ObdToDoipConverter &Converter()
            {
                return *mConverter;
            }

            /// @brief Poll until the DoIP client has fetched the logical address and the request is queued
            bool TrySendRemotePid(uint8_t pid)
            {
                const std::vector<uint8_t> cPid{pid};

                for (int i = 0; i < cMaxPollIterations; ++i)
                {
                    if (Converter().TryGetResponseAsync(cPid))
                    {
                        return true;
                    }

                    Poller.TryPoll(cPollTimeout);
                }

                return false;
            }

            bool TryWaitForCallback()
            {
                for (int i = 0; i < cMaxPollIterations && !CallbackInvoked; ++i)
                {
                    Poller.TryPoll(cPollTimeout);
                }

                return CallbackInvoked;
            }

            /// @brief Send a follow-up positive request and verify it is the first one delivered
            /// @note The TCP stream is ordered, so the previous response has been processed beforehand.
            void ExpectOnlyFollowUpDelivered()
            {
                const uint8_t cFollowUpPid{0x46};
                const uint8_t cFollowUpData{0x55};

                Server.SetUdsResponse({0x62, 0xf5, cFollowUpPid, cFollowUpData});
                ASSERT_TRUE(TrySendRemotePid(cFollowUpPid));
                ASSERT_TRUE(TryWaitForCallback());

                const std::vector<uint8_t> cExpectedPid{cFollowUpPid};
                const std::vector<uint8_t> cExpectedData{cFollowUpData};
                EXPECT_EQ(cExpectedPid, ReceivedPid);
                EXPECT_EQ(cExpectedData, ReceivedData);
            }
        };

        const uint8_t ObdToDoipConverterTest::cService;
        const uint8_t ObdToDoipConverterTest::cProtocolVersion;
        const uint16_t ObdToDoipConverterTest::cLogicalAddress;

        TEST_F(ObdToDoipConverterTest, Constructor)
        {
            EXPECT_EQ(cService, Converter().GetService());
        }

        TEST_F(ObdToDoipConverterTest, TryGetResponseMethod)
        {
            const std::vector<uint8_t> cPid{0x0c};
            std::vector<uint8_t> _response;

            EXPECT_FALSE(Converter().TryGetResponse(cPid, _response));
            EXPECT_TRUE(_response.empty());
        }

        TEST_F(ObdToDoipConverterTest, LocalPid)
        {
            const std::vector<uint8_t> cEngineSpeedPid{0x0c};
            const std::vector<uint8_t> cExpectedData{0x20, 0x00};

            EXPECT_TRUE(Converter().TryGetResponseAsync(cEngineSpeedPid));
            EXPECT_TRUE(CallbackInvoked);
            EXPECT_EQ(cEngineSpeedPid, ReceivedPid);
            EXPECT_EQ(cExpectedData, ReceivedData);
            EXPECT_EQ(cService, ReceivedService);
        }

        TEST_F(ObdToDoipConverterTest, SupportedPidsQuery)
        {
            const std::vector<uint8_t> cSupportedPidsPid{0x00};
            const std::vector<uint8_t> cExpectedData{0x3e, 0x7f, 0xf0, 0x15};

            EXPECT_TRUE(Converter().TryGetResponseAsync(cSupportedPidsPid));
            EXPECT_EQ(cExpectedData, ReceivedData);
        }

        TEST_F(ObdToDoipConverterTest, UnsupportedPid)
        {
            const std::vector<uint8_t> cUnsupportedPid{0xff};

            EXPECT_FALSE(Converter().TryGetResponseAsync(cUnsupportedPid));
            EXPECT_FALSE(CallbackInvoked);
        }

        TEST_F(ObdToDoipConverterTest, RemotePidBeforeVehicleIdentification)
        {
            const std::vector<uint8_t> cVehicleSpeedPid{0x0d};

            // No poll has happened yet, so the logical address is not fetched from the server.
            EXPECT_FALSE(Converter().TryGetResponseAsync(cVehicleSpeedPid));
        }

        TEST_F(ObdToDoipConverterTest, RemotePidOverDoip)
        {
            const uint8_t cVehicleSpeedPid{0x0d};
            const uint8_t cPositiveResponseSid{0x62};
            const uint8_t cDidMsb{0xf5};
            const uint8_t cVehicleSpeed{0x50};

            Server.SetUdsResponse({cPositiveResponseSid, cDidMsb, cVehicleSpeedPid, cVehicleSpeed});

            ASSERT_TRUE(TrySendRemotePid(cVehicleSpeedPid));
            ASSERT_TRUE(TryWaitForCallback());

            const std::vector<uint8_t> cExpectedPid{cVehicleSpeedPid};
            const std::vector<uint8_t> cExpectedData{cVehicleSpeed};
            EXPECT_EQ(cExpectedPid, ReceivedPid);
            EXPECT_EQ(cExpectedData, ReceivedData);
            EXPECT_EQ(cService, ReceivedService);
        }

        TEST_F(ObdToDoipConverterTest, NegativeUdsResponse)
        {
            const uint8_t cFuelLevelPid{0x2f};
            const uint8_t cNegativeResponseSid{0x7f};
            const uint8_t cReadDataByIdentifierSid{0x22};
            const uint8_t cRequestOutOfRangeNrc{0x31};

            Server.SetUdsResponse({cNegativeResponseSid, cReadDataByIdentifierSid, cRequestOutOfRangeNrc});

            ASSERT_TRUE(TrySendRemotePid(cFuelLevelPid));
            ExpectOnlyFollowUpDelivered();
        }

        TEST_F(ObdToDoipConverterTest, TruncatedUdsResponse)
        {
            const uint8_t cOdometerPid{0xa6};
            const uint8_t cPositiveResponseSid{0x62};

            Server.SetUdsResponse({cPositiveResponseSid, 0xf5});

            ASSERT_TRUE(TrySendRemotePid(cOdometerPid));
            ExpectOnlyFollowUpDelivered();
        }
    }
}
