#ifndef MOCK_HTTP_SERVER_H
#define MOCK_HTTP_SERVER_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <atomic>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace application
{
    namespace fixture
    {
        /// @brief Minimal HTTP/1.1 server on the loopback interface serving canned responses
        class MockHttpServer
        {
        private:
            struct Response
            {
                int status;
                std::string body;
            };

            int mListener;
            uint16_t mPort;
            std::atomic_bool mRunning;
            mutable std::mutex mMutex;
            std::map<std::string, Response> mResponses;
            std::vector<std::string> mRequests;
            std::thread mThread;

            static std::string getPath(const std::string &request)
            {
                const size_t cStart{request.find(' ')};
                if (cStart == std::string::npos)
                {
                    return "";
                }
                const size_t cEnd{request.find(' ', cStart + 1)};
                return request.substr(cStart + 1, cEnd - cStart - 1);
            }

            void handle(int connection)
            {
                struct timeval _timeout{1, 0};
                setsockopt(
                    connection, SOL_SOCKET, SO_RCVTIMEO, &_timeout, sizeof(_timeout));

                std::string _request;
                char _buffer[1024];
                while (_request.find("\r\n\r\n") == std::string::npos)
                {
                    const ssize_t cReceived{recv(connection, _buffer, sizeof(_buffer), 0)};
                    if (cReceived <= 0)
                    {
                        return;
                    }
                    _request.append(_buffer, static_cast<size_t>(cReceived));
                }

                Response _response{404, "{}"};
                {
                    std::lock_guard<std::mutex> _lock(mMutex);
                    mRequests.push_back(_request);
                    auto _itr{mResponses.find(getPath(_request))};
                    if (_itr != mResponses.end())
                    {
                        _response = _itr->second;
                    }
                }

                const std::string cRawResponse{
                    "HTTP/1.1 " + std::to_string(_response.status) + " Mock\r\n"
                    "Content-Type: application/json\r\n"
                    "Content-Length: " + std::to_string(_response.body.size()) + "\r\n"
                    "Connection: close\r\n\r\n" + _response.body};

                size_t _sent{0};
                while (_sent < cRawResponse.size())
                {
                    const ssize_t cSent{
                        send(connection, cRawResponse.data() + _sent,
                             cRawResponse.size() - _sent, MSG_NOSIGNAL)};
                    if (cSent <= 0)
                    {
                        return;
                    }
                    _sent += static_cast<size_t>(cSent);
                }
            }

            void serve()
            {
                const int cPollTimeoutMs{20};
                while (mRunning)
                {
                    struct pollfd _descriptor{mListener, POLLIN, 0};
                    if (poll(&_descriptor, 1, cPollTimeoutMs) <= 0)
                    {
                        continue;
                    }

                    const int cConnection{accept(mListener, nullptr, nullptr)};
                    if (cConnection >= 0)
                    {
                        handle(cConnection);
                        shutdown(cConnection, SHUT_RDWR);
                        close(cConnection);
                    }
                }
            }

        public:
            MockHttpServer() : mListener{socket(AF_INET, SOCK_STREAM, 0)},
                               mPort{0},
                               mRunning{true}
            {
                struct sockaddr_in _address{};
                _address.sin_family = AF_INET;
                _address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
                _address.sin_port = 0;
                socklen_t _length{sizeof(_address)};

                if (mListener < 0 ||
                    bind(mListener, (struct sockaddr *)&_address, sizeof(_address)) != 0 ||
                    listen(mListener, 8) != 0 ||
                    getsockname(mListener, (struct sockaddr *)&_address, &_length) != 0)
                {
                    throw std::runtime_error("Mock HTTP server setup failed.");
                }

                mPort = ntohs(_address.sin_port);
                mThread = std::thread(&MockHttpServer::serve, this);
            }

            MockHttpServer(const MockHttpServer &) = delete;
            MockHttpServer &operator=(const MockHttpServer &) = delete;

            uint16_t GetPort() const noexcept
            {
                return mPort;
            }

            std::string GetUrl(const std::string &path = "") const
            {
                return "http://127.0.0.1:" + std::to_string(mPort) + path;
            }

            void SetResponse(const std::string &path, std::string body, int status = 200)
            {
                std::lock_guard<std::mutex> _lock(mMutex);
                mResponses[path] = Response{status, std::move(body)};
            }

            size_t GetRequestCount() const
            {
                std::lock_guard<std::mutex> _lock(mMutex);
                return mRequests.size();
            }

            std::string GetLastRequest() const
            {
                std::lock_guard<std::mutex> _lock(mMutex);
                return mRequests.empty() ? "" : mRequests.back();
            }

            ~MockHttpServer()
            {
                mRunning = false;
                if (mThread.joinable())
                {
                    mThread.join();
                }
                close(mListener);
            }
        };
    }
}

#endif
