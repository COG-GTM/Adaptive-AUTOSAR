#include <gtest/gtest.h>
#include <stdexcept>
#include "../../../../src/ara/com/entry/entry_deserializer.h"

namespace ara
{
    namespace com
    {
        namespace entry
        {
            static void expectTruncatedPayloadsThrow(const std::vector<uint8_t> &payload)
            {
                for (std::size_t _length = 0; _length < payload.size(); ++_length)
                {
                    const std::vector<uint8_t> cTruncatedPayload(
                        payload.begin(), payload.begin() + _length);
                    std::size_t _offset{0};
                    uint8_t _firstOptionNo{0};
                    uint8_t _secondOptionsNo{0};

                    SCOPED_TRACE("Truncated length = " + std::to_string(_length));
                    EXPECT_THROW(
                        EntryDeserializer::Deserialize(
                            cTruncatedPayload, _offset, _firstOptionNo, _secondOptionsNo),
                        std::out_of_range);
                }
            }

            TEST(EntryDeserializerTest, EmptyPayload)
            {
                const std::vector<uint8_t> cPayload;
                std::size_t _offset{0};
                uint8_t _firstOptionNo{0};
                uint8_t _secondOptionsNo{0};

                EXPECT_THROW(
                    EntryDeserializer::Deserialize(
                        cPayload, _offset, _firstOptionNo, _secondOptionsNo),
                    std::out_of_range);
            }

            TEST(EntryDeserializerTest, TruncatedServiceEntry)
            {
                auto _entry{
                    ServiceEntry::CreateOfferServiceEntry(0x0001, 0x0002, 0x03, 0x00000004)};
                uint8_t _optionIndex{0};
                const auto cPayload{_entry->Payload(_optionIndex)};

                expectTruncatedPayloadsThrow(cPayload);
            }

            TEST(EntryDeserializerTest, TruncatedEventgroupEntry)
            {
                auto _entry{
                    EventgroupEntry::CreateSubscribeEventEntry(0x0001, 0x0002, 0x03, 0x04, 0x0005)};
                uint8_t _optionIndex{0};
                const auto cPayload{_entry->Payload(_optionIndex)};

                expectTruncatedPayloadsThrow(cPayload);
            }

            TEST(EntryDeserializerTest, OffsetBeyondPayload)
            {
                auto _entry{
                    ServiceEntry::CreateFindServiceEntry(0x0001, 0x000002, 0x0003, 0x04, 0x00000005)};
                uint8_t _optionIndex{0};
                const auto cPayload{_entry->Payload(_optionIndex)};
                std::size_t _offset{cPayload.size()};
                uint8_t _firstOptionNo{0};
                uint8_t _secondOptionsNo{0};

                EXPECT_THROW(
                    EntryDeserializer::Deserialize(
                        cPayload, _offset, _firstOptionNo, _secondOptionsNo),
                    std::out_of_range);
            }

            TEST(EntryDeserializerTest, UnsupportedEntryType)
            {
                const uint8_t cUnsupportedType{0x02};
                auto _entry{
                    ServiceEntry::CreateFindServiceEntry(0x0001, 0x000002, 0x0003, 0x04, 0x00000005)};
                uint8_t _optionIndex{0};
                auto _payload{_entry->Payload(_optionIndex)};
                _payload.at(0) = cUnsupportedType;

                std::size_t _offset{0};
                uint8_t _firstOptionNo{0};
                uint8_t _secondOptionsNo{0};

                EXPECT_THROW(
                    EntryDeserializer::Deserialize(
                        _payload, _offset, _firstOptionNo, _secondOptionsNo),
                    std::out_of_range);
            }

            TEST(EntryDeserializerTest, CompletePayloadAtOffset)
            {
                const std::vector<uint8_t> cPrefix{0xff, 0xff, 0xff};
                auto _originalEntry{
                    ServiceEntry::CreateFindServiceEntry(0x0001, 0x000002, 0x0003, 0x04, 0x00000005)};
                uint8_t _optionIndex{0};
                const auto cEntryPayload{_originalEntry->Payload(_optionIndex)};

                std::vector<uint8_t> _payload{cPrefix};
                _payload.insert(_payload.end(), cEntryPayload.begin(), cEntryPayload.end());

                std::size_t _offset{cPrefix.size()};
                uint8_t _firstOptionNo{0};
                uint8_t _secondOptionsNo{0};

                auto _deserializedEntry{
                    EntryDeserializer::Deserialize(
                        _payload, _offset, _firstOptionNo, _secondOptionsNo)};

                ASSERT_NE(_deserializedEntry, nullptr);
                EXPECT_EQ(_deserializedEntry->Type(), _originalEntry->Type());
                EXPECT_EQ(_deserializedEntry->ServiceId(), _originalEntry->ServiceId());
                EXPECT_EQ(_offset, _payload.size());
            }
        }
    }
}
