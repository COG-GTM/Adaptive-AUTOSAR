#ifndef DOIP_TEST_HELPER_H
#define DOIP_TEST_HELPER_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <stdexcept>
#include <string>
#include <thread>
#include <asyncbsdsocket/poller.h>
#include <doiplib/controller_config.h>

namespace application
{
    namespace doip
    {
        /// @brief Loopback HTTP stub that stands in for the connected vehicle RESTful API
        /// @note The stub replies to every request with the same JSON body.
        class StubHttpServer
        {
        private:
            const int cPollTimeoutMs{20};

            int mSocket;
            uint16_t mPort;
            std::atomic_bool mRunning;
            std::atomic_int mRequestCount;
            std::string mBody;
            std::thread mThread;

            void serve()
            {
                while (mRunning)
                {
                    struct pollfd _pollFd;
                    _pollFd.fd = mSocket;
                    _pollFd.events = POLLIN;
                    _pollFd.revents = 0;

                    const int cReady{poll(&_pollFd, 1, cPollTimeoutMs)};
                    if (cReady <= 0)
                    {
                        continue;
                    }

                    const int cConnection{accept(mSocket, nullptr, nullptr)};
                    if (cConnection < 0)
                    {
                        continue;
                    }

                    // An idle peer should not block the stub (and its teardown).
                    struct timeval _receiveTimeout;
                    _receiveTimeout.tv_sec = 1;
                    _receiveTimeout.tv_usec = 0;
                    setsockopt(
                        cConnection,
                        SOL_SOCKET,
                        SO_RCVTIMEO,
                        &_receiveTimeout,
                        sizeof(_receiveTimeout));

                    char _buffer[2048];
                    const ssize_t cReceived{
                        recv(cConnection, _buffer, sizeof(_buffer), 0)};

                    if (cReceived > 0)
                    {
                        ++mRequestCount;
                        const std::string cResponse{
                            "HTTP/1.1 200 OK\r\n"
                            "Content-Type: application/json\r\n"
                            "Content-Length: " +
                            std::to_string(mBody.size()) +
                            "\r\nConnection: close\r\n\r\n" + mBody};
                        send(
                            cConnection,
                            cResponse.c_str(),
                            cResponse.size(),
                            MSG_NOSIGNAL);
                    }

                    close(cConnection);
                }
            }

        public:
            /// @brief Constructor
            /// @param body JSON body to be served for all the requests
            /// @throws std::runtime_error Throws when the loopback socket setup failed
            explicit StubHttpServer(std::string body) : mSocket{socket(AF_INET, SOCK_STREAM, 0)},
                                                        mPort{0},
                                                        mRunning{true},
                                                        mRequestCount{0},
                                                        mBody{body}
            {
                struct sockaddr_in _address;
                _address.sin_family = AF_INET;
                _address.sin_addr.s_addr = inet_addr("127.0.0.1");
                _address.sin_port = 0;
                socklen_t _length{sizeof(_address)};

                const bool cSuccessful{
                    mSocket >= 0 &&
                    bind(mSocket, reinterpret_cast<struct sockaddr *>(&_address), _length) == 0 &&
                    listen(mSocket, 4) == 0 &&
                    getsockname(mSocket, reinterpret_cast<struct sockaddr *>(&_address), &_length) == 0};

                if (!cSuccessful)
                {
                    if (mSocket >= 0)
                    {
                        close(mSocket);
                    }

                    throw std::runtime_error("HTTP stub setup failed.");
                }

                // libcurl should reach the stub directly even if a proxy is configured.
                setenv("no_proxy", "127.0.0.1", 1);
                setenv("NO_PROXY", "127.0.0.1", 1);

                mPort = ntohs(_address.sin_port);
                mThread = std::thread(&StubHttpServer::serve, this);
            }

            /// @brief Stop the stub
            /// @note The destruction waits for the serving thread to finish.
            ~StubHttpServer()
            {
                mRunning = false;
                mThread.join();
                close(mSocket);
            }

            /// @brief Base URL of the stub
            std::string GetUrl() const
            {
                return "http://127.0.0.1:" + std::to_string(mPort);
            }

            /// @brief Number of the HTTP requests that the stub has served
            int GetRequestCount() const
            {
                return mRequestCount;
            }
        };

        /// @brief Find a loopback TCP port that is free at the time of the call
        inline uint16_t GetFreePort()
        {
            const int cSocket{socket(AF_INET, SOCK_STREAM, 0)};
            struct sockaddr_in _address;
            _address.sin_family = AF_INET;
            _address.sin_addr.s_addr = inet_addr("127.0.0.1");
            _address.sin_port = 0;
            socklen_t _length{sizeof(_address)};

            const bool cSuccessful{
                cSocket >= 0 &&
                bind(cSocket, reinterpret_cast<struct sockaddr *>(&_address), _length) == 0 &&
                getsockname(cSocket, reinterpret_cast<struct sockaddr *>(&_address), &_length) == 0};

            if (cSocket >= 0)
            {
                close(cSocket);
            }

            if (!cSuccessful)
            {
                throw std::runtime_error("No free loopback port.");
            }

            return ntohs(_address.sin_port);
        }

        /// @brief Poll till a condition is met or the timeout is elapsed
        /// @returns True if the condition is met in time; otherwise false
        inline bool PollUntil(
            const AsyncBsdSocketLib::Poller &poller,
            const std::function<bool()> &condition,
            std::chrono::milliseconds timeout = std::chrono::milliseconds(3000))
        {
            const int cPollTimeoutMs{5};
            const std::chrono::steady_clock::time_point cDeadline{
                std::chrono::steady_clock::now() + timeout};

            while (!condition())
            {
                if (std::chrono::steady_clock::now() > cDeadline)
                {
                    return false;
                }

                poller.TryPoll(cPollTimeoutMs);
            }

            return true;
        }

        /// @brief Poll for a fixed duration to let pending traffic settle
        inline void PollFor(
            const AsyncBsdSocketLib::Poller &poller,
            std::chrono::milliseconds duration)
        {
            const std::function<bool()> cNever{[]() { return false; }};
            PollUntil(poller, cNever, duration);
        }

        /// @brief Create a DoIP controller configuration for the unit tests
        inline DoipLib::ControllerConfig CreateControllerConfig(
            uint8_t protocolVersion)
        {
            DoipLib::ControllerConfig _result;
            _result.protocolVersion = protocolVersion;
            _result.doipMaxRequestBytes = 64;
            _result.doIPInitialVehicleAnnouncementTime = std::chrono::seconds(0);
            _result.doIPVehicleAnnouncementCount = 3;
            _result.doIPVehicleAnnouncementInterval = std::chrono::seconds(1);

            return _result;
        }
    }
}

#endif
