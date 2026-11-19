#include <gtest/gtest.h>
#include <asyncbsdsocket/poller.h>
#include "../../../../../src/ara/com/someip/rpc/socket_rpc_client.h"
#include "../../../../../src/ara/com/someip/rpc/socket_rpc_server.h"

namespace ara
{
    namespace com
    {
        namespace someip
        {
            namespace rpc
            {
                /// @note The tests use real TCP sockets over the loopback interface.
                class SocketRpcTest : public testing::Test
                {
                protected:
                    static const std::string cIpAddress;
                    static const uint8_t cProtocolVersion{1};
                    static const uint8_t cInterfaceVersion{1};
                    static const uint16_t cServiceId{0x1234};
                    static const uint16_t cMethodId{0x0001};
                    static const uint16_t cClientId{0x0042};
                    static const int cPollingTimeoutMs{10};
                    static const int cPollingRetries{200};

                    AsyncBsdSocketLib::Poller Poller;

                    template <typename Predicate>
                    bool PollUntil(Predicate predicate)
                    {
                        for (int i = 0; i < cPollingRetries && !predicate(); ++i)
                        {
                            Poller.TryPoll(cPollingTimeoutMs);
                        }

                        return predicate();
                    }
                };

                const std::string SocketRpcTest::cIpAddress{"127.0.0.1"};
                const uint8_t SocketRpcTest::cProtocolVersion;
                const uint8_t SocketRpcTest::cInterfaceVersion;
                const uint16_t SocketRpcTest::cServiceId;
                const uint16_t SocketRpcTest::cMethodId;
                const uint16_t SocketRpcTest::cClientId;
                const int SocketRpcTest::cPollingTimeoutMs;
                const int SocketRpcTest::cPollingRetries;

                TEST_F(SocketRpcTest, ServerConstructor)
                {
                    const uint16_t cPort{39301};

                    EXPECT_NO_THROW(
                        SocketRpcServer _server(
                            &Poller, cIpAddress, cPort, cProtocolVersion, cInterfaceVersion));
                }

                TEST_F(SocketRpcTest, ServerSetupFailure)
                {
                    // 192.0.2.1 (TEST-NET-1) is not assigned to any local interface.
                    const std::string cForeignIpAddress{"192.0.2.1"};
                    const uint16_t cPort{39302};

                    EXPECT_THROW(
                        SocketRpcServer _server(
                            &Poller, cForeignIpAddress, cPort, cProtocolVersion),
                        std::runtime_error);
                }

                TEST_F(SocketRpcTest, ClientConnectionFailure)
                {
                    // No server is listening to the port.
                    const uint16_t cPort{39303};

                    EXPECT_THROW(
                        SocketRpcClient _client(
                            &Poller, cIpAddress, cPort, cProtocolVersion),
                        std::runtime_error);
                }

                TEST_F(SocketRpcTest, RequestResponseScenario)
                {
                    const uint16_t cPort{39304};
                    const std::vector<uint8_t> cRequestRpcPayload{0x01, 0x02, 0x03};
                    const std::vector<uint8_t> cResponseRpcPayload{0x0a, 0x0b};

                    std::vector<uint8_t> _serverReceivedPayload;
                    const RpcServer::HandlerType cServerHandler{
                        [&](const std::vector<uint8_t> &rpcRequestPayload,
                            std::vector<uint8_t> &rpcResponsePayload)
                        {
                            _serverReceivedPayload = rpcRequestPayload;
                            rpcResponsePayload = cResponseRpcPayload;
                            return true;
                        }};

                    bool _responseReceived{false};
                    SomeIpMessageType _responseType{SomeIpMessageType::Error};
                    SomeIpReturnCode _responseCode{SomeIpReturnCode::eNotOk};
                    uint16_t _responseClientId{0};
                    std::vector<uint8_t> _clientReceivedPayload;
                    const RpcClient::HandlerType cClientHandler{
                        [&](const SomeIpRpcMessage &message)
                        {
                            _responseReceived = true;
                            _responseType = message.MessageType();
                            _responseCode = message.ReturnCode();
                            _responseClientId = message.ClientId();
                            _clientReceivedPayload = message.RpcPayload();
                        }};

                    SocketRpcServer _server(
                        &Poller, cIpAddress, cPort, cProtocolVersion, cInterfaceVersion);
                    _server.SetHandler(cServiceId, cMethodId, cServerHandler);

                    SocketRpcClient _client(
                        &Poller, cIpAddress, cPort, cProtocolVersion, cInterfaceVersion);
                    _client.SetHandler(cServiceId, cMethodId, cClientHandler);
                    // The public Send overload is hidden by the SocketRpcClient override, so call it via the base class.
                    RpcClient &_rpcClient{_client};

                    _rpcClient.Send(cServiceId, cMethodId, cClientId, cRequestRpcPayload);

                    const bool cSucceed{
                        PollUntil(
                            [&]()
                            {
                                return _responseReceived;
                            })};

                    ASSERT_TRUE(cSucceed);
                    EXPECT_EQ(cRequestRpcPayload, _serverReceivedPayload);
                    EXPECT_EQ(SomeIpMessageType::Response, _responseType);
                    EXPECT_EQ(SomeIpReturnCode::eOK, _responseCode);
                    EXPECT_EQ(cClientId, _responseClientId);
                    EXPECT_EQ(cResponseRpcPayload, _clientReceivedPayload);
                }

                TEST_F(SocketRpcTest, UnknownMethodScenario)
                {
                    const uint16_t cPort{39305};
                    const uint16_t cUnknownMethodId{0x0002};
                    const std::vector<uint8_t> cRequestRpcPayload{0x01};

                    bool _serverHandlerCalled{false};
                    const RpcServer::HandlerType cServerHandler{
                        [&](const std::vector<uint8_t> &,
                            std::vector<uint8_t> &)
                        {
                            _serverHandlerCalled = true;
                            return true;
                        }};

                    bool _responseReceived{false};
                    SomeIpMessageType _responseType{SomeIpMessageType::Response};
                    SomeIpReturnCode _responseCode{SomeIpReturnCode::eOK};
                    const RpcClient::HandlerType cClientHandler{
                        [&](const SomeIpRpcMessage &message)
                        {
                            _responseReceived = true;
                            _responseType = message.MessageType();
                            _responseCode = message.ReturnCode();
                        }};

                    SocketRpcServer _server(
                        &Poller, cIpAddress, cPort, cProtocolVersion, cInterfaceVersion);
                    _server.SetHandler(cServiceId, cMethodId, cServerHandler);

                    SocketRpcClient _client(
                        &Poller, cIpAddress, cPort, cProtocolVersion, cInterfaceVersion);
                    _client.SetHandler(cServiceId, cUnknownMethodId, cClientHandler);
                    // The public Send overload is hidden by the SocketRpcClient override, so call it via the base class.
                    RpcClient &_rpcClient{_client};

                    _rpcClient.Send(
                        cServiceId, cUnknownMethodId, cClientId, cRequestRpcPayload);

                    const bool cSucceed{
                        PollUntil(
                            [&]()
                            {
                                return _responseReceived;
                            })};

                    ASSERT_TRUE(cSucceed);
                    EXPECT_FALSE(_serverHandlerCalled);
                    EXPECT_EQ(SomeIpMessageType::Error, _responseType);
                    EXPECT_EQ(SomeIpReturnCode::eUnknownMethod, _responseCode);
                }

                TEST_F(SocketRpcTest, ConsecutiveRequestsScenario)
                {
                    const uint16_t cPort{39306};
                    const std::vector<uint8_t> cRequestRpcPayload{0x01};
                    const size_t cRequests{3};

                    const RpcServer::HandlerType cServerHandler{
                        [](const std::vector<uint8_t> &rpcRequestPayload,
                           std::vector<uint8_t> &rpcResponsePayload)
                        {
                            rpcResponsePayload = rpcRequestPayload;
                            return true;
                        }};

                    std::vector<uint16_t> _sessionIds;
                    const RpcClient::HandlerType cClientHandler{
                        [&](const SomeIpRpcMessage &message)
                        {
                            _sessionIds.push_back(message.SessionId());
                        }};

                    SocketRpcServer _server(
                        &Poller, cIpAddress, cPort, cProtocolVersion, cInterfaceVersion);
                    _server.SetHandler(cServiceId, cMethodId, cServerHandler);

                    SocketRpcClient _client(
                        &Poller, cIpAddress, cPort, cProtocolVersion, cInterfaceVersion);
                    _client.SetHandler(cServiceId, cMethodId, cClientHandler);
                    // The public Send overload is hidden by the SocketRpcClient override, so call it via the base class.
                    RpcClient &_rpcClient{_client};

                    for (size_t i = 0; i < cRequests; ++i)
                    {
                        _rpcClient.Send(cServiceId, cMethodId, cClientId, cRequestRpcPayload);

                        const size_t cExpectedResponses{i + 1};
                        const bool cSucceed{
                            PollUntil(
                                [&]()
                                {
                                    return _sessionIds.size() == cExpectedResponses;
                                })};
                        ASSERT_TRUE(cSucceed);
                    }

                    const std::vector<uint16_t> cExpectedSessionIds{1, 2, 3};
                    EXPECT_EQ(cExpectedSessionIds, _sessionIds);
                }
            }
        }
    }
}
