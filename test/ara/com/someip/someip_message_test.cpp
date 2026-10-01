#include <gtest/gtest.h>
#include <limits>
#include "../../../../src/ara/com/someip/someip_message.h"

namespace ara
{
    namespace com
    {
        namespace someip
        {
            class TestableSomeIpMessage : public SomeIpMessage
            {
            public:
                static const uint32_t cHeaderLength{8};

                TestableSomeIpMessage(uint32_t messageId,
                                      uint16_t clientId,
                                      uint8_t protocolVersion,
                                      uint8_t interfaceVersion,
                                      SomeIpMessageType messageType,
                                      uint16_t sessionId = 1) : SomeIpMessage(messageId,
                                                                              clientId,
                                                                              protocolVersion,
                                                                              interfaceVersion,
                                                                              messageType,
                                                                              sessionId)
                {
                }

                TestableSomeIpMessage(uint32_t messageId,
                                      uint16_t clientId,
                                      uint8_t protocolVersion,
                                      uint8_t interfaceVersion,
                                      SomeIpMessageType messageType,
                                      SomeIpReturnCode returnCode,
                                      uint16_t sessionId = 1) : SomeIpMessage(messageId,
                                                                              clientId,
                                                                              protocolVersion,
                                                                              interfaceVersion,
                                                                              messageType,
                                                                              returnCode,
                                                                              sessionId)
                {
                }

                uint32_t Length() const noexcept override
                {
                    return cHeaderLength;
                }

                static void Deserialize(
                    TestableSomeIpMessage *message,
                    const std::vector<uint8_t> &payload)
                {
                    SomeIpMessage::Deserialize(message, payload);
                }
            };

            const uint32_t TestableSomeIpMessage::cHeaderLength;

            class SomeIpMessageTest : public testing::Test
            {
            protected:
                const uint32_t cMessageId{0x12345678};
                const uint16_t cClientId{0x9abc};
                const uint16_t cSessionId{0xdef0};
                const uint8_t cProtocolVersion{0x01};
                const uint8_t cInterfaceVersion{0x02};
            };

            TEST_F(SomeIpMessageTest, RequestConstructor)
            {
                const uint16_t cDefaultSessionId{1};
                const SomeIpMessageType cMessageType{SomeIpMessageType::Request};

                TestableSomeIpMessage _message(
                    cMessageId, cClientId, cProtocolVersion, cInterfaceVersion, cMessageType);

                EXPECT_EQ(cMessageId, _message.MessageId());
                EXPECT_EQ(cClientId, _message.ClientId());
                EXPECT_EQ(cDefaultSessionId, _message.SessionId());
                EXPECT_EQ(cProtocolVersion, _message.ProtocolVersion());
                EXPECT_EQ(cInterfaceVersion, _message.InterfaceVersion());
                EXPECT_EQ(cMessageType, _message.MessageType());
                EXPECT_EQ(SomeIpReturnCode::eOK, _message.ReturnCode());
            }

            TEST_F(SomeIpMessageTest, NotificationConstructor)
            {
                const SomeIpMessageType cMessageType{SomeIpMessageType::Notification};

                TestableSomeIpMessage _message(
                    cMessageId, cClientId, cProtocolVersion, cInterfaceVersion,
                    cMessageType, cSessionId);

                EXPECT_EQ(cSessionId, _message.SessionId());
                EXPECT_EQ(cMessageType, _message.MessageType());
                EXPECT_EQ(SomeIpReturnCode::eOK, _message.ReturnCode());
            }

            TEST_F(SomeIpMessageTest, InvalidRequestMessageType)
            {
                EXPECT_THROW(
                    TestableSomeIpMessage(
                        cMessageId, cClientId, cProtocolVersion, cInterfaceVersion,
                        SomeIpMessageType::Response),
                    std::invalid_argument);

                EXPECT_THROW(
                    TestableSomeIpMessage(
                        cMessageId, cClientId, cProtocolVersion, cInterfaceVersion,
                        SomeIpMessageType::TpRequest),
                    std::invalid_argument);
            }

            TEST_F(SomeIpMessageTest, ResponseConstructor)
            {
                const SomeIpMessageType cMessageType{SomeIpMessageType::Response};
                const SomeIpReturnCode cReturnCode{SomeIpReturnCode::eOK};

                TestableSomeIpMessage _message(
                    cMessageId, cClientId, cProtocolVersion, cInterfaceVersion,
                    cMessageType, cReturnCode, cSessionId);

                EXPECT_EQ(cSessionId, _message.SessionId());
                EXPECT_EQ(cMessageType, _message.MessageType());
                EXPECT_EQ(cReturnCode, _message.ReturnCode());
            }

            TEST_F(SomeIpMessageTest, ErrorConstructor)
            {
                const SomeIpMessageType cMessageType{SomeIpMessageType::Error};
                const SomeIpReturnCode cReturnCode{SomeIpReturnCode::eUnknownMethod};

                TestableSomeIpMessage _message(
                    cMessageId, cClientId, cProtocolVersion, cInterfaceVersion,
                    cMessageType, cReturnCode);

                EXPECT_EQ(cMessageType, _message.MessageType());
                EXPECT_EQ(cReturnCode, _message.ReturnCode());
            }

            TEST_F(SomeIpMessageTest, InvalidResponseMessageType)
            {
                EXPECT_THROW(
                    TestableSomeIpMessage(
                        cMessageId, cClientId, cProtocolVersion, cInterfaceVersion,
                        SomeIpMessageType::Request, SomeIpReturnCode::eOK),
                    std::invalid_argument);

                EXPECT_THROW(
                    TestableSomeIpMessage(
                        cMessageId, cClientId, cProtocolVersion, cInterfaceVersion,
                        SomeIpMessageType::Notification, SomeIpReturnCode::eOK),
                    std::invalid_argument);
            }

            TEST_F(SomeIpMessageTest, ErrorWithOkReturnCode)
            {
                EXPECT_THROW(
                    TestableSomeIpMessage(
                        cMessageId, cClientId, cProtocolVersion, cInterfaceVersion,
                        SomeIpMessageType::Error, SomeIpReturnCode::eOK),
                    std::invalid_argument);
            }

            TEST_F(SomeIpMessageTest, SetSessionIdMethod)
            {
                TestableSomeIpMessage _message(
                    cMessageId, cClientId, cProtocolVersion, cInterfaceVersion,
                    SomeIpMessageType::Request);

                _message.SetSessionId(cSessionId);
                EXPECT_EQ(cSessionId, _message.SessionId());
            }

            TEST_F(SomeIpMessageTest, IncrementSessionIdMethod)
            {
                const uint16_t cInitialSessionId{1};
                const uint16_t cExpectedSessionId{2};

                TestableSomeIpMessage _message(
                    cMessageId, cClientId, cProtocolVersion, cInterfaceVersion,
                    SomeIpMessageType::Request, cInitialSessionId);

                EXPECT_FALSE(_message.IncrementSessionId());
                EXPECT_EQ(cExpectedSessionId, _message.SessionId());
            }

            TEST_F(SomeIpMessageTest, IncrementSessionIdWrapping)
            {
                const uint16_t cMaxSessionId{std::numeric_limits<uint16_t>::max()};
                const uint16_t cWrappedSessionId{1};

                TestableSomeIpMessage _message(
                    cMessageId, cClientId, cProtocolVersion, cInterfaceVersion,
                    SomeIpMessageType::Request, cMaxSessionId);

                EXPECT_TRUE(_message.IncrementSessionId());
                EXPECT_EQ(cWrappedSessionId, _message.SessionId());
            }

            TEST_F(SomeIpMessageTest, PayloadMethod)
            {
                const std::vector<uint8_t> cExpectedPayload{
                    0x12, 0x34, 0x56, 0x78,
                    0x00, 0x00, 0x00, 0x08,
                    0x9a, 0xbc,
                    0xde, 0xf0,
                    0x01,
                    0x02,
                    0x81,
                    0x02};

                TestableSomeIpMessage _message(
                    cMessageId, cClientId, cProtocolVersion, cInterfaceVersion,
                    SomeIpMessageType::Error, SomeIpReturnCode::eUnknownService, cSessionId);

                EXPECT_EQ(cExpectedPayload, _message.Payload());
            }

            TEST_F(SomeIpMessageTest, DeserializeMethod)
            {
                const SomeIpMessageType cMessageType{SomeIpMessageType::Error};
                const SomeIpReturnCode cReturnCode{SomeIpReturnCode::eNotReachable};

                TestableSomeIpMessage _originalMessage(
                    cMessageId, cClientId, cProtocolVersion, cInterfaceVersion,
                    cMessageType, cReturnCode, cSessionId);

                TestableSomeIpMessage _deserializedMessage(
                    0, 0, 0, 0, SomeIpMessageType::Request);

                TestableSomeIpMessage::Deserialize(
                    &_deserializedMessage, _originalMessage.Payload());

                EXPECT_EQ(cMessageId, _deserializedMessage.MessageId());
                EXPECT_EQ(cClientId, _deserializedMessage.ClientId());
                EXPECT_EQ(cSessionId, _deserializedMessage.SessionId());
                EXPECT_EQ(cProtocolVersion, _deserializedMessage.ProtocolVersion());
                EXPECT_EQ(cInterfaceVersion, _deserializedMessage.InterfaceVersion());
                EXPECT_EQ(cMessageType, _deserializedMessage.MessageType());
                EXPECT_EQ(cReturnCode, _deserializedMessage.ReturnCode());
            }

            TEST_F(SomeIpMessageTest, MoveSemantics)
            {
                const SomeIpMessageType cMessageType{SomeIpMessageType::Response};
                const SomeIpReturnCode cReturnCode{SomeIpReturnCode::eOK};

                TestableSomeIpMessage _originalMessage(
                    cMessageId, cClientId, cProtocolVersion, cInterfaceVersion,
                    cMessageType, cReturnCode, cSessionId);

                TestableSomeIpMessage _movedMessage{std::move(_originalMessage)};
                EXPECT_EQ(cMessageId, _movedMessage.MessageId());
                EXPECT_EQ(cClientId, _movedMessage.ClientId());
                EXPECT_EQ(cSessionId, _movedMessage.SessionId());
                EXPECT_EQ(cMessageType, _movedMessage.MessageType());

                TestableSomeIpMessage _assignedMessage(
                    0, 0, 0, 0, SomeIpMessageType::Request);
                _assignedMessage = std::move(_movedMessage);
                EXPECT_EQ(cMessageId, _assignedMessage.MessageId());
                EXPECT_EQ(cClientId, _assignedMessage.ClientId());
                EXPECT_EQ(cSessionId, _assignedMessage.SessionId());
                EXPECT_EQ(cProtocolVersion, _assignedMessage.ProtocolVersion());
                EXPECT_EQ(cInterfaceVersion, _assignedMessage.InterfaceVersion());
                EXPECT_EQ(cMessageType, _assignedMessage.MessageType());
                EXPECT_EQ(cReturnCode, _assignedMessage.ReturnCode());
            }
        }
    }
}
