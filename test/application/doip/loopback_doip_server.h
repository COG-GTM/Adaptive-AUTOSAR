#ifndef LOOPBACK_DOIP_SERVER_H
#define LOOPBACK_DOIP_SERVER_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <atomic>
#include <thread>
#include "../../../src/application/doip/doip_server.h"

namespace application
{
    namespace doip
    {
        /// @brief Real DoipServer listening on the loopback interface and polled by a background thread
        /// @note The RESTful resources URL is unreachable on purpose, so no vehicle cloud API is involved.
        class LoopbackDoipServer
        {
        private:
            static const uint8_t cProtocolVersion{0x02};
            static const uint16_t cLogicalAddress{0x0001};
            static const uint64_t cEid{0x010203040506};
            static const uint64_t cGid{0x0a0b0c0d0e0f};

            AsyncBsdSocketLib::Poller mPoller;
            helper::CurlWrapper mCurl;
            DoipServer mServer;
            std::atomic_bool mRunning;
            std::thread mPollingThread;

            static DoipLib::ControllerConfig getConfig()
            {
                DoipLib::ControllerConfig _result;
                _result.protocolVersion = cProtocolVersion;
                _result.doipMaxRequestBytes = DoipServer::cDoipPacketSize;
                _result.doIPInitialVehicleAnnouncementTime = std::chrono::seconds(0);
                _result.doIPVehicleAnnouncementCount = 1;
                _result.doIPVehicleAnnouncementInterval = std::chrono::seconds(1);

                return _result;
            }

            void poll()
            {
                const int cTimeoutMs{10};

                while (mRunning)
                {
                    mPoller.TryPoll(cTimeoutMs);
                }
            }

        public:
            /// @brief Loopback IPv4 address
            static constexpr const char *cIpAddress{"127.0.0.1"};
            /// @brief Unreachable RESTful resources URL (discard port)
            static constexpr const char *cResourcesUrl{"http://127.0.0.1:9/resources"};

            /// @brief Constructor
            /// @param port DoIP server TCP listening port number
            /// @param vin Vehicle Identification Number
            LoopbackDoipServer(
                uint16_t port,
                std::string vin) : mCurl("dummy_api_key", "dummy_bearer_token"),
                                   mServer(&mPoller, &mCurl, cResourcesUrl, cIpAddress, port, getConfig(), std::move(vin), cLogicalAddress, cEid, cGid),
                                   mRunning{true},
                                   mPollingThread(&LoopbackDoipServer::poll, this)
            {
            }

            ~LoopbackDoipServer()
            {
                mRunning = false;
                mPollingThread.join();
            }
        };

        /// @brief Raw TCP tester connected to a DoIP server on the loopback interface
        /// @note The DoIP server serves a single TCP connection in its lifetime, so one client is used per server.
        class LoopbackDoipClient
        {
        private:
            int mSocket;
            bool mConnected;

        public:
            /// @brief Constructor
            /// @param port DoIP server TCP listening port number
            explicit LoopbackDoipClient(uint16_t port) : mSocket{socket(AF_INET, SOCK_STREAM, 0)},
                                                         mConnected{false}
            {
                if (mSocket >= 0)
                {
                    struct timeval _timeout;
                    _timeout.tv_sec = 5;
                    _timeout.tv_usec = 0;
                    setsockopt(
                        mSocket,
                        SOL_SOCKET,
                        SO_RCVTIMEO,
                        &_timeout,
                        sizeof(_timeout));

                    struct sockaddr_in _address;
                    _address.sin_family = AF_INET;
                    _address.sin_port = htons(port);
                    inet_pton(
                        AF_INET,
                        LoopbackDoipServer::cIpAddress,
                        &_address.sin_addr);

                    mConnected =
                        connect(
                            mSocket,
                            reinterpret_cast<struct sockaddr *>(&_address),
                            sizeof(_address)) == 0;
                }
            }

            LoopbackDoipClient(const LoopbackDoipClient &) = delete;
            LoopbackDoipClient &operator=(const LoopbackDoipClient &) = delete;

            ~LoopbackDoipClient()
            {
                if (mSocket >= 0)
                {
                    close(mSocket);
                }
            }

            /// @brief Send a raw DoIP request and receive the raw response
            /// @param[in] request Serialized DoIP request
            /// @param[out] response Serialized DoIP response limited to the length declared within its header
            /// @return True if a complete DoIP response is received; otherwise false
            bool TryExchange(
                const std::vector<uint8_t> &request,
                std::vector<uint8_t> &response)
            {
                const size_t cHeaderSize{8};
                const size_t cPayloadLengthOffset{4};

                if (!mConnected)
                {
                    return false;
                }

                const ssize_t cSentSize{
                    send(mSocket, request.data(), request.size(), 0)};
                if (cSentSize != static_cast<ssize_t>(request.size()))
                {
                    return false;
                }

                // The server always transmits a fixed-size packet per response.
                std::vector<uint8_t> _received;
                while (_received.size() < DoipServer::cDoipPacketSize)
                {
                    uint8_t _buffer[DoipServer::cDoipPacketSize];
                    const size_t cRemainingSize{
                        DoipServer::cDoipPacketSize - _received.size()};
                    const ssize_t cReceivedSize{
                        recv(mSocket, _buffer, cRemainingSize, 0)};

                    if (cReceivedSize <= 0)
                    {
                        return false;
                    }

                    _received.insert(
                        _received.end(), _buffer, _buffer + cReceivedSize);
                }

                size_t _payloadLength{0};
                for (size_t i = 0; i < sizeof(uint32_t); ++i)
                {
                    _payloadLength <<= 8;
                    _payloadLength |= _received[cPayloadLengthOffset + i];
                }

                const size_t cMessageSize{cHeaderSize + _payloadLength};
                if (cMessageSize > _received.size())
                {
                    return false;
                }

                // Only the header-declared length is meaningful.
                _received.resize(cMessageSize);
                response = std::move(_received);

                return true;
            }
        };
    }
}

#endif
