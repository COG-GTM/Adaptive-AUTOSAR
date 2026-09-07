#include <arpa/inet.h>
#include <cstdio>
#include <fstream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <gtest/gtest.h>
#include "../../../src/ara/telemetry/telemetry_hub.h"
#include "../../../src/ara/telemetry/telemetry_server.h"

namespace ara
{
    namespace telemetry
    {
        class TelemetryServerTest : public testing::Test
        {
        private:
            const std::string cIndexContent{"<html>cockpit</html>"};

        protected:
            const std::string cWebRoot{"telemetry_server_test_web"};

            TelemetryHub mHub;

            std::string Request(const std::string &target)
            {
                const int cDescriptor{::socket(AF_INET, SOCK_STREAM, 0)};
                EXPECT_GE(cDescriptor, 0);

                sockaddr_in _address{};
                _address.sin_family = AF_INET;
                _address.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
                _address.sin_port = ::htons(mServer->Port());

                EXPECT_EQ(
                    0,
                    ::connect(
                        cDescriptor,
                        reinterpret_cast<const sockaddr *>(&_address),
                        sizeof(_address)));

                const std::string cRequest{
                    "GET " + target + " HTTP/1.1\r\nHost: localhost\r\n\r\n"};
                EXPECT_GT(
                    ::send(cDescriptor, cRequest.c_str(), cRequest.size(), 0), 0);

                std::string _response;
                char _buffer[512];
                ssize_t _received;

                while ((_received = ::recv(cDescriptor, _buffer, sizeof(_buffer), 0)) > 0)
                {
                    _response.append(_buffer, static_cast<std::size_t>(_received));
                }

                ::close(cDescriptor);

                return _response;
            }

            std::unique_ptr<TelemetryServer> mServer;

            void SetUp() override
            {
                ::mkdir(cWebRoot.c_str(), 0755);
                std::ofstream _indexStream(cWebRoot + "/index.html");
                _indexStream << cIndexContent;
                _indexStream.close();

                mServer.reset(new TelemetryServer(&mHub, 0, cWebRoot));
                ASSERT_TRUE(mServer->Start());
            }

            void TearDown() override
            {
                mServer->Stop();
                mServer.reset();

                std::remove((cWebRoot + "/index.html").c_str());
                ::rmdir(cWebRoot.c_str());
            }
        };

        TEST_F(TelemetryServerTest, HubActivation)
        {
            EXPECT_TRUE(mHub.Enabled());
            EXPECT_NE(0, mServer->Port());
        }

        TEST_F(TelemetryServerTest, TelemetryEndpoint)
        {
            mHub.PublishExecutionState("ExecutionManagement", "kRunning");
            const std::string cResponse{Request("/api/telemetry")};

            EXPECT_NE(std::string::npos, cResponse.find("200 OK"));
            EXPECT_NE(std::string::npos, cResponse.find("application/json"));
            EXPECT_NE(std::string::npos, cResponse.find("ExecutionManagement"));
        }

        TEST_F(TelemetryServerTest, DashboardServing)
        {
            const std::string cResponse{Request("/")};

            EXPECT_NE(std::string::npos, cResponse.find("200 OK"));
            EXPECT_NE(std::string::npos, cResponse.find("text/html"));
            EXPECT_NE(std::string::npos, cResponse.find("<html>cockpit</html>"));
        }

        TEST_F(TelemetryServerTest, MissingFile)
        {
            const std::string cResponse{Request("/missing.js")};

            EXPECT_NE(std::string::npos, cResponse.find("404 Not Found"));
        }

        TEST_F(TelemetryServerTest, PathTraversalRejection)
        {
            const std::string cResponse{Request("/../CMakeLists.txt")};

            EXPECT_NE(std::string::npos, cResponse.find("403 Forbidden"));
        }
    }
}
