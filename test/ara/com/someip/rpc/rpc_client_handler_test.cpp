#include <gtest/gtest.h>
#include "../../../../../src/ara/com/someip/rpc/rpc_client.h"

namespace ara
{
    namespace com
    {
        namespace someip
        {
            namespace rpc
            {
                class RpcClientHandlerTest : public testing::Test, public RpcClient
                {
                protected:
                    static const uint8_t cProtocolVersion{1};
                    static const uint8_t cInterfaceVersion{1};
                    const uint16_t cServiceId{1};
                    const uint16_t cMethodId{2};
                    const uint16_t cClientId{3};

                    std::vector<uint8_t> LastRequest;
                    uint32_t HandlerCalls;
                    std::vector<uint8_t> HandledRpcPayload;

                    RpcClientHandlerTest() : RpcClient(cProtocolVersion, cInterfaceVersion),
                                             HandlerCalls{0}
                    {
                    }

                    void Send(const std::vector<uint8_t> &payload) override
                    {
                        LastRequest = payload;
                    }

                    using RpcClient::Send;

                    void RegisterHandler()
                    {
                        const HandlerType cHandler{
                            [this](const SomeIpRpcMessage &message)
                            {
                                ++HandlerCalls;
                                HandledRpcPayload = message.RpcPayload();
                            }};
                        SetHandler(cServiceId, cMethodId, cHandler);
                    }

                    std::vector<uint8_t> CreateResponse(
                        uint16_t serviceId, uint16_t methodId,
                        const std::vector<uint8_t> &rpcPayload) const
                    {
                        const uint16_t cSessionId{1};
                        uint32_t _messageId{static_cast<uint32_t>(serviceId << 16)};
                        _messageId |= methodId;
                        const SomeIpRpcMessage cResponse(
                            _messageId,
                            cClientId,
                            cSessionId,
                            cProtocolVersion,
                            cInterfaceVersion,
                            SomeIpReturnCode::eOK,
                            rpcPayload);

                        return cResponse.Payload();
                    }
                };

                const uint8_t RpcClientHandlerTest::cProtocolVersion;
                const uint8_t RpcClientHandlerTest::cInterfaceVersion;

                TEST_F(RpcClientHandlerTest, RequestSerialization)
                {
                    const std::vector<uint8_t> cRpcPayload{0xde, 0xad};
                    const std::vector<uint8_t> cExpectedRequest{
                        0x00, 0x01, 0x00, 0x02,
                        0x00, 0x00, 0x00, 0x0a,
                        0x00, 0x03, 0x00, 0x01,
                        0x01, 0x01, 0x00, 0x00,
                        0xde, 0xad};

                    Send(cServiceId, cMethodId, cClientId, cRpcPayload);

                    EXPECT_EQ(cExpectedRequest, LastRequest);
                }

                TEST_F(RpcClientHandlerTest, RegisteredHandlerInvocation)
                {
                    const std::vector<uint8_t> cRpcPayload{0x01, 0x02, 0x03};
                    RegisterHandler();

                    InvokeHandler(CreateResponse(cServiceId, cMethodId, cRpcPayload));

                    EXPECT_EQ(1U, HandlerCalls);
                    EXPECT_EQ(cRpcPayload, HandledRpcPayload);
                }

                TEST_F(RpcClientHandlerTest, UnknownMethodResponse)
                {
                    const uint16_t cUnknownMethodId{0x7fff};
                    const std::vector<uint8_t> cRpcPayload{0x01};
                    RegisterHandler();

                    EXPECT_NO_THROW(
                        InvokeHandler(
                            CreateResponse(cServiceId, cUnknownMethodId, cRpcPayload)));

                    EXPECT_EQ(0U, HandlerCalls);
                }

                TEST_F(RpcClientHandlerTest, NoHandlerRegistered)
                {
                    const std::vector<uint8_t> cRpcPayload;

                    EXPECT_NO_THROW(
                        InvokeHandler(
                            CreateResponse(cServiceId, cMethodId, cRpcPayload)));

                    EXPECT_EQ(0U, HandlerCalls);
                }

                TEST_F(RpcClientHandlerTest, TruncatedResponse)
                {
                    // The response is shorter than the SOME/IP length field offset.
                    const std::vector<uint8_t> cCorruptedResponse{0x00, 0x01, 0x00};
                    RegisterHandler();

                    EXPECT_NO_THROW(InvokeHandler(cCorruptedResponse));

                    EXPECT_EQ(0U, HandlerCalls);
                }

                TEST_F(RpcClientHandlerTest, HandlerReplacement)
                {
                    const std::vector<uint8_t> cRpcPayload{0x01};
                    uint32_t _replacementCalls{0};
                    const HandlerType cReplacement{
                        [&_replacementCalls](const SomeIpRpcMessage &)
                        {
                            ++_replacementCalls;
                        }};
                    RegisterHandler();
                    SetHandler(cServiceId, cMethodId, cReplacement);

                    InvokeHandler(CreateResponse(cServiceId, cMethodId, cRpcPayload));

                    EXPECT_EQ(0U, HandlerCalls);
                    EXPECT_EQ(1U, _replacementCalls);
                }
            }
        }
    }
}
