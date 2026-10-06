#include <gtest/gtest.h>
#include "../../../src/application/helper/curl_wrapper.h"
#include "./mock_http_server.h"
#include "./testable_fixture.h"

namespace application
{
    namespace helper
    {
        TEST(CurlWrapperTest, Constructor)
        {
            EXPECT_NO_THROW(CurlWrapper _curl("api-key", "bearer-token"));
        }

        TEST(CurlWrapperTest, SuccessfulRequest)
        {
            fixture::MockHttpServer _server;
            _server.SetResponse("/vehicles", "{\"vehicles\":[]}");
            CurlWrapper _curl("api-key", "bearer-token");

            std::string _response;
            EXPECT_TRUE(_curl.TryExecute(_server.GetUrl("/vehicles"), &_response));
            EXPECT_EQ("{\"vehicles\":[]}", _response);
        }

        TEST(CurlWrapperTest, RequestHeaders)
        {
            fixture::MockHttpServer _server;
            _server.SetResponse("/vehicles", "{}");
            CurlWrapper _curl("api-key", "bearer-token");

            std::string _response;
            ASSERT_TRUE(_curl.TryExecute(_server.GetUrl("/vehicles"), &_response));

            const std::string cRequest{_server.GetLastRequest()};
            EXPECT_EQ(0, cRequest.find("GET /vehicles HTTP/1.1\r\n"));
            EXPECT_NE(std::string::npos, cRequest.find("accept: application/json\r\n"));
            EXPECT_NE(std::string::npos, cRequest.find("vcc-api-key: api-key\r\n"));
            EXPECT_NE(std::string::npos, cRequest.find("Authorization: Bearer bearer-token\r\n"));
        }

        TEST(CurlWrapperTest, ConsecutiveRequests)
        {
            fixture::MockHttpServer _server;
            _server.SetResponse("/first", "1");
            _server.SetResponse("/second", "2");
            CurlWrapper _curl("api-key", "bearer-token");

            std::string _response;
            EXPECT_TRUE(_curl.TryExecute(_server.GetUrl("/first"), &_response));
            EXPECT_EQ("1", _response);
            EXPECT_TRUE(_curl.TryExecute(_server.GetUrl("/second"), &_response));
            EXPECT_EQ("2", _response);
            EXPECT_EQ(2, _server.GetRequestCount());
        }

        TEST(CurlWrapperTest, NullResponseBuffer)
        {
            fixture::MockHttpServer _server;
            _server.SetResponse("/vehicles", "{}");
            CurlWrapper _curl("api-key", "bearer-token");

            EXPECT_TRUE(_curl.TryExecute(_server.GetUrl("/vehicles"), nullptr));
        }

        TEST(CurlWrapperTest, ConnectionRefused)
        {
            const uint16_t cUnusedPort{fixture::GetFreePort()};
            CurlWrapper _curl("api-key", "bearer-token");

            std::string _response{"unchanged"};
            EXPECT_FALSE(
                _curl.TryExecute(
                    "http://127.0.0.1:" + std::to_string(cUnusedPort) + "/vehicles",
                    &_response));
            EXPECT_EQ("unchanged", _response);
        }

        TEST(CurlWrapperTest, MalformedUrl)
        {
            CurlWrapper _curl("api-key", "bearer-token");
            std::string _response;
            EXPECT_FALSE(_curl.TryExecute("unsupported://url", &_response));
        }
    }
}
