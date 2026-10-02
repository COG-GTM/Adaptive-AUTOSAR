#include <functional>
#include <gtest/gtest.h>
#include <asyncbsdsocket/poller.h>
#include <asyncbsdsocket/tcp_listener.h>
#include <doiplib/diag_message.h>
#include <doiplib/diag_message_ack.h>
#include <doiplib/doip_controller.h>
#include <doiplib/vehicle_id_request.h>
#include <doiplib/vehicle_id_response.h>
#include "../../../src/application/doip/obd_to_doip_converter.h"

namespace application
{
    namespace doip
    {
        namespace
        {
            const uint8_t cProtocolVersion{0x02};
            const uint16_t cLogicalAddress{0x0e80};
            const std::string cLocalhost{"127.0.0.1"};

            class StubVehicleIdRequestHandler : public DoipLib::MessageHandler
            {
            private:
                DoipLib::VehicleIdRequest mRequest;
                const DoipLib::VehicleIdResponse mResponse;

            public:
                StubVehicleIdRequestHandler() : mResponse(
                                                    cProtocolVersion,
                                                    std::string("WVWZZZ1JZXW000001"),
                                                    cLogicalAddress,
                                                    {0x01, 0x02, 0x03, 0x04, 0x05, 0x06},
                                                    {0x06, 0x05, 0x04, 0x03, 0x02, 0x01},
                                                    0x00)
                {
                }

                DoipLib::Message *GetMessage() override
                {
                    return &mRequest;
                }

                bool TryHandle(
                    const DoipLib::Message *request,
                    std::vector<uint8_t> &response) const override
                {
                    if (dynamic_cast<const DoipLib::VehicleIdRequest *>(request))
                    {
                        mResponse.Serialize(response);
                        return true;
                    }

                    return false;
                }
            };

            /// @brief Stub UDS server which records requests and replies with a canned response
            class StubDiagMessageHandler : public DoipLib::MessageHandler
            {
            private:
                DoipLib::DiagMessage mRequest;

            public:
                mutable std::vector<std::vector<uint8_t>> Requests;
                std::function<std::vector<uint8_t>(const std::vector<uint8_t> &)> Responder;

                DoipLib::Message *GetMessage() override
                {
                    return &mRequest;
                }

                bool TryHandle(
                    const DoipLib::Message *request,
                    std::vector<uint8_t> &response) const override
                {
                    auto _diagMessage{dynamic_cast<const DoipLib::DiagMessage *>(request)};
                    if (!_diagMessage)
                    {
                        return false;
                    }

                    std::vector<uint8_t> _userData;
                    _diagMessage->GetUserData(_userData);
                    Requests.push_back(_userData);

                    const DoipLib::DiagMessageAck cAck(
                        cProtocolVersion,
                        _diagMessage->GetSourceAddress(),
                        _diagMessage->GetTargetAddress(),
                        Responder(_userData));
                    cAck.Serialize(response);

                    return true;
                }
            };

            DoipLib::ControllerConfig getControllerConfig()
            {
                DoipLib::ControllerConfig _result;
                _result.protocolVersion = cProtocolVersion;
                _result.doipMaxRequestBytes = 64;
                _result.doIPInitialVehicleAnnouncementTime = std::chrono::seconds(0);
                _result.doIPVehicleAnnouncementCount = 3;
                _result.doIPVehicleAnnouncementInterval = std::chrono::seconds(1);

                return _result;
            }

            /// @brief Minimal loopback DoIP server answering vehicle ID and diagnostic messages
            class StubDoipServer
            {
            private:
                static constexpr size_t cPacketSize{64};

                AsyncBsdSocketLib::Poller *const mPoller;
                AsyncBsdSocketLib::TcpListener mListener;
                DoipLib::DoipController mController;
                StubVehicleIdRequestHandler mVehicleIdRequestHandler;
                std::vector<std::vector<uint8_t>> mSendQueue;

                void onAccept()
                {
                    if (mListener.TryAccept() && mListener.TryMakeConnectionNonblock())
                    {
                        mPoller->TryAddReceiver(&mListener, std::bind(&StubDoipServer::onReceive, this));
                        mPoller->TryAddSender(&mListener, std::bind(&StubDoipServer::onSend, this));
                    }
                }

                void onReceive()
                {
                    std::array<uint8_t, cPacketSize> _buffer;
                    if (mListener.Receive(_buffer) > 0)
                    {
                        const std::vector<uint8_t> cRequest(_buffer.cbegin(), _buffer.cend());
                        std::vector<uint8_t> _response;
                        if (mController.TryHandle(cRequest, _response))
                        {
                            mSendQueue.push_back(std::move(_response));
                        }
                    }
                }

                void onSend()
                {
                    for (const auto &cResponse : mSendQueue)
                    {
                        std::array<uint8_t, cPacketSize> _buffer{};
                        std::copy(cResponse.cbegin(), cResponse.cend(), _buffer.begin());
                        mListener.Send(_buffer);
                    }
                    mSendQueue.clear();
                }

            public:
                StubDiagMessageHandler DiagMessageHandler;

                StubDoipServer(AsyncBsdSocketLib::Poller *poller, uint16_t port) : mPoller{poller},
                                                                                   mListener(cLocalhost, port),
                                                                                   mController(getControllerConfig())
                {
                    if (!mListener.TrySetup() ||
                        !mPoller->TryAddListener(&mListener, std::bind(&StubDoipServer::onAccept, this)))
                    {
                        throw std::runtime_error("Stub DoIP server setup failed.");
                    }

                    mController.Register(
                        DoipLib::PayloadType::VehicleIdRequest, &mVehicleIdRequestHandler);
                    mController.Register(
                        DoipLib::PayloadType::DiagMessage, &DiagMessageHandler);
                }

                ~StubDoipServer()
                {
                    mPoller->TryRemoveSender(&mListener);
                    mPoller->TryRemoveReceiver(&mListener);
                    mPoller->TryRemoveListener(&mListener);
                }
            };

            constexpr size_t StubDoipServer::cPacketSize;
        }

        class ObdToDoipConverterTest : public testing::Test
        {
        private:
            static const int cMaxPollIterations{200};
            static const int cPollTimeoutMs{10};

        protected:
            static const uint8_t cObdService{0x01};

            struct ObdResponse
            {
                std::vector<uint8_t> Pid;
                std::vector<uint8_t> Data;
                uint8_t Service;
            };

            AsyncBsdSocketLib::Poller Poller;
            std::vector<ObdResponse> Responses;

            bool PollUntil(std::function<bool()> predicate)
            {
                for (int i = 0; i < cMaxPollIterations; ++i)
                {
                    if (predicate())
                    {
                        return true;
                    }

                    Poller.TryPoll(cPollTimeoutMs);
                }

                return false;
            }

            void PollFor(int iterations)
            {
                for (int i = 0; i < iterations; ++i)
                {
                    Poller.TryPoll(cPollTimeoutMs);
                }
            }

            void SetCallback(ObdToDoipConverter &converter)
            {
                converter.SetCallback(
                    [this](const std::vector<uint8_t> &pid, std::vector<uint8_t> &&data, uint8_t service)
                    { Responses.push_back({pid, std::move(data), service}); });
            }

            static std::vector<uint8_t> PositiveResponder(const std::vector<uint8_t> &request)
            {
                const uint8_t cPositiveSid{0x62};
                const uint8_t cObdData{0x42};
                return {cPositiveSid, request.at(1), request.at(2), cObdData};
            }
        };

        const uint8_t ObdToDoipConverterTest::cObdService;

        TEST_F(ObdToDoipConverterTest, ConstructorWithoutServer)
        {
            const uint16_t cPort{18441};
            EXPECT_THROW(
                ObdToDoipConverter _converter(&Poller, cLocalhost, cPort),
                std::runtime_error);
        }

        TEST_F(ObdToDoipConverterTest, GetService)
        {
            const uint16_t cPort{18442};
            StubDoipServer _server(&Poller, cPort);
            ObdToDoipConverter _converter(&Poller, cLocalhost, cPort);

            EXPECT_EQ(cObdService, _converter.GetService());
        }

        TEST_F(ObdToDoipConverterTest, TryGetResponseIsUnsupported)
        {
            const uint16_t cPort{18443};
            const int cPollIterations{20};
            StubDoipServer _server(&Poller, cPort);
            ObdToDoipConverter _converter(&Poller, cLocalhost, cPort);
            _server.DiagMessageHandler.Responder = PositiveResponder;

            PollFor(cPollIterations);

            // The converter handles queries only asynchronously
            std::vector<uint8_t> _response;
            EXPECT_FALSE(_converter.TryGetResponse({0x0d}, _response));
            EXPECT_FALSE(_converter.TryGetResponse({0x0c}, _response));
            EXPECT_TRUE(_response.empty());

            PollFor(cPollIterations);
            EXPECT_TRUE(_server.DiagMessageHandler.Requests.empty());
        }

        TEST_F(ObdToDoipConverterTest, TryGetResponseAsyncLocalPid)
        {
            const uint16_t cPort{18444};
            const int cPollIterations{20};
            const std::vector<uint8_t> cPid{0x0c};
            const std::vector<uint8_t> cExpectedData{0x20, 0x00};

            StubDoipServer _server(&Poller, cPort);
            ObdToDoipConverter _converter(&Poller, cLocalhost, cPort);
            _server.DiagMessageHandler.Responder = PositiveResponder;
            SetCallback(_converter);

            ASSERT_TRUE(_converter.TryGetResponseAsync(cPid));
            ASSERT_EQ(1U, Responses.size());
            EXPECT_EQ(cPid, Responses.front().Pid);
            EXPECT_EQ(cExpectedData, Responses.front().Data);
            EXPECT_EQ(cObdService, Responses.front().Service);

            // Locally emulated PIDs must not be forwarded via DoIP
            PollFor(cPollIterations);
            EXPECT_TRUE(_server.DiagMessageHandler.Requests.empty());
        }

        TEST_F(ObdToDoipConverterTest, TryGetResponseAsyncUnsupportedPid)
        {
            const uint16_t cPort{18445};
            StubDoipServer _server(&Poller, cPort);
            ObdToDoipConverter _converter(&Poller, cLocalhost, cPort);
            SetCallback(_converter);

            EXPECT_FALSE(_converter.TryGetResponseAsync({0x01}));
            EXPECT_TRUE(Responses.empty());
        }

        TEST_F(ObdToDoipConverterTest, TryGetResponseAsyncRemotePidBeforeHandshake)
        {
            const uint16_t cPort{18446};
            StubDoipServer _server(&Poller, cPort);
            ObdToDoipConverter _converter(&Poller, cLocalhost, cPort);
            SetCallback(_converter);

            // The logical address is unknown before the vehicle ID response is polled
            EXPECT_FALSE(_converter.TryGetResponseAsync({0x0d}));
        }

        TEST_F(ObdToDoipConverterTest, TryGetResponseAsyncRemotePids)
        {
            const uint16_t cPort{18447};
            const std::vector<uint8_t> cRemotePids{0x0d, 0x2f, 0x46, 0x5e, 0x05, 0xa6};
            const uint8_t cUdsSid{0x22};
            const uint8_t cDidMsb{0xf5};
            const uint8_t cObdData{0x42};

            StubDoipServer _server(&Poller, cPort);
            ObdToDoipConverter _converter(&Poller, cLocalhost, cPort);
            _server.DiagMessageHandler.Responder = PositiveResponder;
            SetCallback(_converter);

            for (size_t i = 0; i < cRemotePids.size(); ++i)
            {
                const std::vector<uint8_t> cPid{cRemotePids[i]};
                ASSERT_TRUE(PollUntil([&]()
                                      { return _converter.TryGetResponseAsync(cPid); }));
                ASSERT_TRUE(PollUntil([&]()
                                      { return Responses.size() == i + 1; }));

                const std::vector<uint8_t> cExpectedUdsRequest{cUdsSid, cDidMsb, cRemotePids[i]};
                ASSERT_EQ(i + 1, _server.DiagMessageHandler.Requests.size());
                EXPECT_EQ(cExpectedUdsRequest, _server.DiagMessageHandler.Requests.back());

                const std::vector<uint8_t> cExpectedData{cObdData};
                EXPECT_EQ(cPid, Responses.back().Pid);
                EXPECT_EQ(cExpectedData, Responses.back().Data);
                EXPECT_EQ(cObdService, Responses.back().Service);
            }
        }

        TEST_F(ObdToDoipConverterTest, TryGetResponseAsyncNegativeUdsResponse)
        {
            const uint16_t cPort{18448};
            const int cPollIterations{20};
            const std::vector<uint8_t> cPid{0x0d};
            const std::vector<uint8_t> cExpectedUdsRequest{0x22, 0xf5, 0x0d};

            StubDoipServer _server(&Poller, cPort);
            ObdToDoipConverter _converter(&Poller, cLocalhost, cPort);
            _server.DiagMessageHandler.Responder = [](const std::vector<uint8_t> &request)
            {
                return std::vector<uint8_t>{0x7f, request.at(0), 0x31};
            };
            SetCallback(_converter);

            ASSERT_TRUE(PollUntil([&]()
                                  { return _converter.TryGetResponseAsync(cPid); }));
            ASSERT_TRUE(PollUntil([&]()
                                  { return !_server.DiagMessageHandler.Requests.empty(); }));
            EXPECT_EQ(cExpectedUdsRequest, _server.DiagMessageHandler.Requests.front());

            PollFor(cPollIterations);
            EXPECT_TRUE(Responses.empty());
        }
    }
}
