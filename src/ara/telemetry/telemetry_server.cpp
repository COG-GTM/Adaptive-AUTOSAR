#include <arpa/inet.h>
#include <chrono>
#include <fstream>
#include <netinet/in.h>
#include <poll.h>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>
#include "./telemetry_hub.h"
#include "./telemetry_server.h"

namespace ara
{
    namespace telemetry
    {
        const int64_t TelemetryServer::cStreamIntervalMs;

        namespace
        {
            const int cAcceptPollTimeoutMs{200};
            const std::size_t cRequestBufferSize{2048};

            std::string contentTypeOf(const std::string &path)
            {
                auto _dotPosition{path.rfind('.')};
                const std::string cExtension{
                    _dotPosition == std::string::npos
                        ? std::string()
                        : path.substr(_dotPosition)};

                if (cExtension == ".html")
                {
                    return "text/html; charset=utf-8";
                }
                else if (cExtension == ".css")
                {
                    return "text/css; charset=utf-8";
                }
                else if (cExtension == ".js")
                {
                    return "application/javascript; charset=utf-8";
                }
                else if (cExtension == ".svg")
                {
                    return "image/svg+xml";
                }
                else if (cExtension == ".json")
                {
                    return "application/json; charset=utf-8";
                }
                else
                {
                    return "application/octet-stream";
                }
            }

            bool tryParseTarget(const std::string &request, std::string &target)
            {
                const std::string cMethod{"GET "};

                if (request.compare(0, cMethod.size(), cMethod) != 0)
                {
                    return false;
                }

                const auto cTargetStart{cMethod.size()};
                const auto cTargetEnd{request.find(' ', cTargetStart)};

                if (cTargetEnd == std::string::npos)
                {
                    return false;
                }

                target = request.substr(cTargetStart, cTargetEnd - cTargetStart);

                return !target.empty();
            }

            uint64_t parseSinceSequence(const std::string &query)
            {
                const std::string cKey{"since="};
                const auto cKeyPosition{query.find(cKey)};

                if (cKeyPosition == std::string::npos)
                {
                    return 0;
                }

                const std::string cValue{
                    query.substr(cKeyPosition + cKey.size())};

                try
                {
                    return static_cast<uint64_t>(std::stoull(cValue));
                }
                catch (const std::exception &)
                {
                    return 0;
                }
            }
        }

        TelemetryServer::TelemetryServer(
            TelemetryHub *hub,
            uint16_t port,
            std::string webRoot) : mHub{hub},
                                   mWebRoot{std::move(webRoot)},
                                   mPort{port},
                                   mListenDescriptor{-1},
                                   mRunning{false}
        {
        }

        TelemetryServer::~TelemetryServer() noexcept
        {
            Stop();
        }

        uint16_t TelemetryServer::Port() const noexcept
        {
            return mPort;
        }

        bool TelemetryServer::Start()
        {
            if (mRunning)
            {
                return false;
            }

            mListenDescriptor = ::socket(AF_INET, SOCK_STREAM, 0);
            if (mListenDescriptor < 0)
            {
                return false;
            }

            const int cEnabled{1};
            ::setsockopt(
                mListenDescriptor,
                SOL_SOCKET,
                SO_REUSEADDR,
                &cEnabled,
                sizeof(cEnabled));

            sockaddr_in _address{};
            _address.sin_family = AF_INET;
            _address.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
            _address.sin_port = ::htons(mPort);

            if (::bind(
                    mListenDescriptor,
                    reinterpret_cast<const sockaddr *>(&_address),
                    sizeof(_address)) < 0)
            {
                ::close(mListenDescriptor);
                mListenDescriptor = -1;

                return false;
            }

            if (::listen(mListenDescriptor, 8) < 0)
            {
                ::close(mListenDescriptor);
                mListenDescriptor = -1;

                return false;
            }

            sockaddr_in _boundAddress{};
            socklen_t _boundLength{sizeof(_boundAddress)};
            if (::getsockname(
                    mListenDescriptor,
                    reinterpret_cast<sockaddr *>(&_boundAddress),
                    &_boundLength) == 0)
            {
                mPort = ::ntohs(_boundAddress.sin_port);
            }

            mRunning = true;
            mHub->Enable();
            mAcceptingThread = std::thread{&TelemetryServer::accepting, this};

            return true;
        }

        void TelemetryServer::Stop() noexcept
        {
            if (!mRunning)
            {
                return;
            }

            mRunning = false;

            if (mAcceptingThread.joinable())
            {
                mAcceptingThread.join();
            }

            joinConnections();

            if (mListenDescriptor >= 0)
            {
                ::close(mListenDescriptor);
                mListenDescriptor = -1;
            }
        }

        void TelemetryServer::joinConnections()
        {
            for (auto &connectionThread : mConnectionThreads)
            {
                if (connectionThread.joinable())
                {
                    connectionThread.join();
                }
            }

            mConnectionThreads.clear();
        }

        void TelemetryServer::accepting()
        {
            while (mRunning)
            {
                pollfd _descriptor{};
                _descriptor.fd = mListenDescriptor;
                _descriptor.events = POLLIN;

                const int cPollResult{
                    ::poll(&_descriptor, 1, cAcceptPollTimeoutMs)};

                if (cPollResult <= 0)
                {
                    continue;
                }

                const int cClientDescriptor{
                    ::accept(mListenDescriptor, nullptr, nullptr)};

                if (cClientDescriptor < 0)
                {
                    continue;
                }

                mConnectionThreads.emplace_back(
                    &TelemetryServer::handleConnection, this, cClientDescriptor);
            }
        }

        bool TelemetryServer::sendAll(
            int clientDescriptor, const std::string &payload)
        {
            std::size_t _sent{0};

            while (_sent < payload.size())
            {
                const ssize_t cResult{
                    ::send(
                        clientDescriptor,
                        payload.data() + _sent,
                        payload.size() - _sent,
                        MSG_NOSIGNAL)};

                if (cResult <= 0)
                {
                    return false;
                }

                _sent += static_cast<std::size_t>(cResult);
            }

            return true;
        }

        void TelemetryServer::respond(
            int clientDescriptor,
            const std::string &status,
            const std::string &contentType,
            const std::string &body)
        {
            std::ostringstream _response;
            _response << "HTTP/1.1 " << status << "\r\n"
                      << "Content-Type: " << contentType << "\r\n"
                      << "Content-Length: " << body.size() << "\r\n"
                      << "Cache-Control: no-store\r\n"
                      << "Connection: close\r\n\r\n"
                      << body;

            sendAll(clientDescriptor, _response.str());
        }

        void TelemetryServer::serveFile(
            int clientDescriptor, const std::string &path)
        {
            const std::string cRelativePath{path == "/" ? "/index.html" : path};

            if (cRelativePath.find("..") != std::string::npos)
            {
                respond(
                    clientDescriptor,
                    "403 Forbidden",
                    "text/plain; charset=utf-8",
                    "Forbidden");

                return;
            }

            const std::string cFilePath{mWebRoot + cRelativePath};
            std::ifstream _fileStream(cFilePath, std::ifstream::binary);

            if (!_fileStream.is_open())
            {
                respond(
                    clientDescriptor,
                    "404 Not Found",
                    "text/plain; charset=utf-8",
                    "Not found: " + cRelativePath);

                return;
            }

            std::ostringstream _content;
            _content << _fileStream.rdbuf();

            respond(
                clientDescriptor,
                "200 OK",
                contentTypeOf(cRelativePath),
                _content.str());
        }

        void TelemetryServer::streamTelemetry(int clientDescriptor)
        {
            const std::string cHeader{
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/event-stream\r\n"
                "Cache-Control: no-store\r\n"
                "Connection: keep-alive\r\n\r\n"};

            if (!sendAll(clientDescriptor, cHeader))
            {
                return;
            }

            uint64_t _sinceSequence{0};

            while (mRunning)
            {
                const uint64_t cSequence{mHub->Sequence()};
                const std::string cSnapshot{
                    mHub->SerializeSnapshot(_sinceSequence)};
                _sinceSequence = cSequence;

                if (!sendAll(clientDescriptor, "data: " + cSnapshot + "\n\n"))
                {
                    return;
                }

                std::this_thread::sleep_for(
                    std::chrono::milliseconds(cStreamIntervalMs));
            }
        }

        void TelemetryServer::handleConnection(int clientDescriptor)
        {
            std::string _request;
            _request.resize(cRequestBufferSize);

            const ssize_t cReceived{
                ::recv(clientDescriptor, &_request[0], _request.size(), 0)};

            if (cReceived > 0)
            {
                _request.resize(static_cast<std::size_t>(cReceived));

                std::string _target;
                if (!tryParseTarget(_request, _target))
                {
                    respond(
                        clientDescriptor,
                        "400 Bad Request",
                        "text/plain; charset=utf-8",
                        "Bad request");
                }
                else
                {
                    const auto cQueryPosition{_target.find('?')};
                    const std::string cPath{_target.substr(0, cQueryPosition)};
                    const std::string cQuery{
                        cQueryPosition == std::string::npos
                            ? std::string()
                            : _target.substr(cQueryPosition + 1)};

                    if (cPath == "/api/stream")
                    {
                        streamTelemetry(clientDescriptor);
                    }
                    else if (cPath == "/api/telemetry")
                    {
                        respond(
                            clientDescriptor,
                            "200 OK",
                            "application/json; charset=utf-8",
                            mHub->SerializeSnapshot(parseSinceSequence(cQuery)));
                    }
                    else
                    {
                        serveFile(clientDescriptor, cPath);
                    }
                }
            }

            ::shutdown(clientDescriptor, SHUT_RDWR);
            ::close(clientDescriptor);
        }
    }
}
