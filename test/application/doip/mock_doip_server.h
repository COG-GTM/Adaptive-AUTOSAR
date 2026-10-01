#ifndef MOCK_DOIP_SERVER_H
#define MOCK_DOIP_SERVER_H

#include <array>
#include <stdexcept>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <asyncbsdsocket/poller.h>
#include <asyncbsdsocket/tcp_listener.h>
#include <doiplib/diag_message.h>
#include <doiplib/diag_message_ack.h>
#include <doiplib/doip_controller.h>
#include "../../../src/ara/com/helper/concurrent_queue.h"
#include "../../../src/application/doip/vehicle_id_request_handler.h"

namespace application
{
    namespace doip
    {
        /// @brief DoIP diagnostic message handler that replies with a canned UDS response
        class MockDiagMessageHandler : public DoipLib::MessageHandler
        {
        private:
            const uint8_t cProtocolVersion;
            DoipLib::DiagMessage mRequest;
            std::vector<uint8_t> mUdsResponse;

        public:
            explicit MockDiagMessageHandler(uint8_t protocolVersion) : cProtocolVersion{protocolVersion}
            {
            }

            void SetUdsResponse(std::vector<uint8_t> &&udsResponse)
            {
                mUdsResponse = std::move(udsResponse);
            }

            DoipLib::Message *GetMessage() override
            {
                return static_cast<DoipLib::Message *>(&mRequest);
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

                DoipLib::DiagMessageAck _diagMessageAck(
                    cProtocolVersion,
                    _diagMessage->GetSourceAddress(),
                    _diagMessage->GetTargetAddress(),
                    mUdsResponse);
                _diagMessageAck.Serialize(response);

                return true;
            }
        };

        /// @brief Loopback DoIP server stub to terminate a DoIP client TCP connection in tests
        class MockDoipServer
        {
        private:
            static constexpr size_t cDoipPacketSize{64};
            static const uint32_t cDoipMaxRequestBytes{64};
            static const uint8_t cAnnouncementCount{3};

            AsyncBsdSocketLib::Poller *const mPoller;
            AsyncBsdSocketLib::TcpListener mListener;
            DoipLib::DoipController mController;
            VehicleIdRequestHandler mVehicleIdRequestHandler;
            MockDiagMessageHandler mDiagMessageHandler;
            ara::com::helper::ConcurrentQueue<std::vector<uint8_t>> mSendQueue;
            uint16_t mPort;

            static DoipLib::ControllerConfig getConfig(uint8_t protocolVersion)
            {
                DoipLib::ControllerConfig _result;
                _result.protocolVersion = protocolVersion;
                _result.doipMaxRequestBytes = cDoipMaxRequestBytes;
                _result.doIPInitialVehicleAnnouncementTime = std::chrono::seconds(0);
                _result.doIPVehicleAnnouncementCount = cAnnouncementCount;
                _result.doIPVehicleAnnouncementInterval = std::chrono::seconds(1);

                return _result;
            }

            void onAccept()
            {
                if (mListener.TryAccept() && mListener.TryMakeConnectionNonblock())
                {
                    mPoller->TryAddReceiver(
                        &mListener, std::bind(&MockDoipServer::onReceive, this));
                    mPoller->TryAddSender(
                        &mListener, std::bind(&MockDoipServer::onSend, this));
                }
            }

            void onReceive()
            {
                std::array<uint8_t, cDoipPacketSize> _receiveBuffer;
                if (mListener.Receive(_receiveBuffer) <= 0)
                {
                    return;
                }

                const std::vector<uint8_t> cRequest(
                    _receiveBuffer.cbegin(), _receiveBuffer.cend());
                std::vector<uint8_t> _response;
                if (mController.TryHandle(cRequest, _response))
                {
                    mSendQueue.TryEnqueue(std::move(_response));
                }
            }

            void onSend()
            {
                while (!mSendQueue.Empty())
                {
                    std::vector<uint8_t> _response;
                    if (mSendQueue.TryDequeue(_response) &&
                        _response.size() <= cDoipPacketSize)
                    {
                        std::array<uint8_t, cDoipPacketSize> _sendBuffer{};
                        std::copy(_response.cbegin(), _response.cend(), _sendBuffer.begin());
                        mListener.Send(_sendBuffer);
                    }
                }
            }

        public:
            /// @brief Constructor
            /// @param poller Poller shared with the DoIP client under test
            /// @param ipAddress Listening IPv4 address
            /// @param protocolVersion DoIP protocol version
            /// @param vin Vehicle Identification Number to announce
            /// @param logicalAddress Logical address to announce
            /// @note The server listens on an ephemeral port to avoid collisions between parallel test processes.
            MockDoipServer(
                AsyncBsdSocketLib::Poller *poller,
                std::string ipAddress,
                uint8_t protocolVersion,
                std::string vin,
                uint16_t logicalAddress) : mPoller{poller},
                                           mListener(ipAddress, 0),
                                           mController(getConfig(protocolVersion)),
                                           mVehicleIdRequestHandler(protocolVersion, std::move(vin), logicalAddress, 0, 0),
                                           mDiagMessageHandler(protocolVersion),
                                           mPort{0}
            {
                if (!mListener.TrySetup())
                {
                    throw std::runtime_error("Mock DoIP server setup failed.");
                }

                struct sockaddr_in _address;
                socklen_t _addressLength{sizeof(_address)};
                if (getsockname(
                        mListener.Descriptor(),
                        reinterpret_cast<struct sockaddr *>(&_address),
                        &_addressLength) != 0)
                {
                    throw std::runtime_error("Mock DoIP server port lookup failed.");
                }
                mPort = ntohs(_address.sin_port);

                if (!mPoller->TryAddListener(
                        &mListener, std::bind(&MockDoipServer::onAccept, this)))
                {
                    throw std::runtime_error("Mock DoIP server poller registration failed.");
                }

                mController.Register(
                    DoipLib::PayloadType::VehicleIdRequest, &mVehicleIdRequestHandler);
                mController.Register(
                    DoipLib::PayloadType::DiagMessage, &mDiagMessageHandler);
            }

            MockDoipServer() = delete;

            /// @brief Get the ephemeral listening port
            uint16_t Port() const noexcept
            {
                return mPort;
            }

            /// @brief Set the UDS response returned for the next diagnostic messages
            void SetUdsResponse(std::vector<uint8_t> &&udsResponse)
            {
                mDiagMessageHandler.SetUdsResponse(std::move(udsResponse));
            }

            ~MockDoipServer()
            {
                mPoller->TryRemoveSender(&mListener);
                mPoller->TryRemoveReceiver(&mListener);
                mPoller->TryRemoveListener(&mListener);
            }
        };
    }
}

#endif
