#ifndef TESTABLE_FIXTURE_H
#define TESTABLE_FIXTURE_H

#include <gtest/gtest.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <asyncbsdsocket/poller.h>

#ifndef APPLICATION_TEST_CONFIGURATION_DIR
#define APPLICATION_TEST_CONFIGURATION_DIR "configuration"
#endif

namespace application
{
    namespace fixture
    {
        /// @brief Get an unused local port by binding to an ephemeral port
        /// @param socketType Socket type (SOCK_STREAM or SOCK_DGRAM)
        inline uint16_t GetFreePort(int socketType = SOCK_STREAM)
        {
            const int cDescriptor{socket(AF_INET, socketType, 0)};
            struct sockaddr_in _address{};
            _address.sin_family = AF_INET;
            _address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            _address.sin_port = 0;
            socklen_t _length{sizeof(_address)};

            if (cDescriptor < 0 ||
                bind(cDescriptor, (struct sockaddr *)&_address, sizeof(_address)) != 0 ||
                getsockname(cDescriptor, (struct sockaddr *)&_address, &_length) != 0)
            {
                if (cDescriptor >= 0)
                {
                    close(cDescriptor);
                }
                throw std::runtime_error("Free port allocation failed.");
            }

            close(cDescriptor);
            return ntohs(_address.sin_port);
        }

        /// @brief Wait until a predicate is satisfied or the timeout expires
        inline bool WaitUntil(
            std::function<bool()> predicate,
            std::chrono::milliseconds timeout = std::chrono::milliseconds(5000))
        {
            const auto cDeadline{std::chrono::steady_clock::now() + timeout};
            while (std::chrono::steady_clock::now() < cDeadline)
            {
                if (predicate())
                {
                    return true;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            return predicate();
        }

        /// @brief Path of a manifest file within the repository configuration directory
        inline std::string GetConfigurationPath(const std::string &manifest)
        {
            return std::string(APPLICATION_TEST_CONFIGURATION_DIR) + "/" + manifest;
        }

        /// @brief Temporary directory that is removed with its content at destruction
        class TemporaryDirectory
        {
        private:
            std::string mPath;

        public:
            TemporaryDirectory()
            {
                char _template[] = "/tmp/application_test_XXXXXX";
                const char *cPath{mkdtemp(_template)};
                if (cPath == nullptr)
                {
                    throw std::runtime_error("Temporary directory creation failed.");
                }
                mPath = cPath;
            }

            TemporaryDirectory(const TemporaryDirectory &) = delete;
            TemporaryDirectory &operator=(const TemporaryDirectory &) = delete;

            const std::string &GetPath() const noexcept
            {
                return mPath;
            }

            std::string GetFilePath(const std::string &filename) const
            {
                return mPath + "/" + filename;
            }

            /// @brief Copy a repository manifest into the directory while replacing text fragments
            /// @throws std::invalid_argument Thrown if a fragment to be replaced does not exist
            std::string WriteManifest(
                const std::string &manifest,
                const std::map<std::string, std::string> &replacements = {}) const
            {
                std::ifstream _input(GetConfigurationPath(manifest));
                std::stringstream _buffer;
                _buffer << _input.rdbuf();
                std::string _content{_buffer.str()};

                for (const auto &replacement : replacements)
                {
                    size_t _position{_content.find(replacement.first)};
                    if (_position == std::string::npos)
                    {
                        throw std::invalid_argument(
                            "Manifest fragment not found: " + replacement.first);
                    }

                    while (_position != std::string::npos)
                    {
                        _content.replace(
                            _position, replacement.first.length(), replacement.second);
                        _position = _content.find(
                            replacement.first, _position + replacement.second.length());
                    }
                }

                const std::string cPath{GetFilePath(manifest)};
                std::ofstream _output(cPath);
                _output << _content;

                return cPath;
            }

            ~TemporaryDirectory()
            {
                const std::string cCommand{"rm -rf '" + mPath + "'"};
                std::system(cCommand.c_str());
            }
        };

        /// @brief Redirect the process standard output to a file for log assertions
        /// @note Captured output is replayed to the original stdout if the current test fails
        class StdoutCapture
        {
        private:
            std::string mPath;
            int mOriginalDescriptor;

            static void flush()
            {
                std::cout.flush();
                std::fflush(stdout);
            }

        public:
            StdoutCapture()
            {
                char _template[] = "/tmp/application_stdout_XXXXXX";
                const int cDescriptor{mkstemp(_template)};
                if (cDescriptor < 0)
                {
                    throw std::runtime_error("Stdout capture file creation failed.");
                }
                mPath = _template;

                flush();
                mOriginalDescriptor = dup(STDOUT_FILENO);
                dup2(cDescriptor, STDOUT_FILENO);
                close(cDescriptor);
            }

            StdoutCapture(const StdoutCapture &) = delete;
            StdoutCapture &operator=(const StdoutCapture &) = delete;

            std::string GetOutput() const
            {
                flush();
                std::ifstream _input(mPath);
                std::stringstream _buffer;
                _buffer << _input.rdbuf();
                return _buffer.str();
            }

            bool Contains(const std::string &text) const
            {
                return GetOutput().find(text) != std::string::npos;
            }

            bool WaitFor(
                const std::string &text,
                std::chrono::milliseconds timeout = std::chrono::milliseconds(5000)) const
            {
                return WaitUntil([this, &text]()
                                 { return Contains(text); },
                                 timeout);
            }

            ~StdoutCapture()
            {
                const std::string cOutput{GetOutput()};
                dup2(mOriginalDescriptor, STDOUT_FILENO);
                close(mOriginalDescriptor);
                unlink(mPath.c_str());

                if (::testing::Test::HasFailure())
                {
                    std::cout << "----- Captured stdout -----\n"
                              << cOutput
                              << "---------------------------" << std::endl;
                }
            }
        };

        /// @brief Background thread that drives a poller
        class PollingThread
        {
        private:
            AsyncBsdSocketLib::Poller *const mPoller;
            std::atomic_bool mRunning;
            std::thread mThread;

        public:
            explicit PollingThread(AsyncBsdSocketLib::Poller *poller) : mPoller{poller},
                                                                        mRunning{false}
            {
            }

            PollingThread(const PollingThread &) = delete;
            PollingThread &operator=(const PollingThread &) = delete;

            void Start()
            {
                if (!mRunning.exchange(true))
                {
                    mThread = std::thread(
                        [this]()
                        {
                            while (mRunning)
                            {
                                mPoller->TryPoll();
                                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                            }
                        });
                }
            }

            void Stop()
            {
                mRunning = false;
                if (mThread.joinable())
                {
                    mThread.join();
                }
            }

            ~PollingThread()
            {
                Stop();
            }
        };
    }
}

#endif
