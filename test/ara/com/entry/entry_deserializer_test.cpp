#include <gtest/gtest.h>
#include "../../../../src/ara/com/entry/entry_deserializer.h"
#include "../../../../src/ara/com/option/ipv4_endpoint_option.h"
#include "../../../../src/ara/com/option/loadbalancing_option.h"

namespace ara
{
    namespace com
    {
        namespace entry
        {
            TEST(EntryDeserializerTest, FindServiceEntry)
            {
                const uint16_t cServiceId{0x1234};
                const uint32_t cTTL{0x00abcd};
                const uint16_t cInstanceId{0x5678};
                const uint8_t cMajorVersion{0x09};
                const uint32_t cMinorVersion{0x0a0b0c0d};

                auto _originalEntry{
                    ServiceEntry::CreateFindServiceEntry(
                        cServiceId, cTTL, cInstanceId, cMajorVersion, cMinorVersion)};

                uint8_t _optionIndex{0};
                const std::vector<uint8_t> cPayload{_originalEntry->Payload(_optionIndex)};

                std::size_t _offset{0};
                uint8_t _numberOfFirstOptions;
                uint8_t _numberOfSecondOptions;
                auto _deserializedEntry{
                    EntryDeserializer::Deserialize(
                        cPayload, _offset, _numberOfFirstOptions, _numberOfSecondOptions)};

                auto _serviceEntry{dynamic_cast<ServiceEntry *>(_deserializedEntry.get())};
                ASSERT_NE(nullptr, _serviceEntry);

                EXPECT_EQ(EntryType::Finding, _serviceEntry->Type());
                EXPECT_EQ(cServiceId, _serviceEntry->ServiceId());
                EXPECT_EQ(cInstanceId, _serviceEntry->InstanceId());
                EXPECT_EQ(cMajorVersion, _serviceEntry->MajorVersion());
                EXPECT_EQ(cTTL, _serviceEntry->TTL());
                EXPECT_EQ(cMinorVersion, _serviceEntry->MinorVersion());
                EXPECT_EQ(0, _numberOfFirstOptions);
                EXPECT_EQ(0, _numberOfSecondOptions);
                EXPECT_EQ(cPayload.size(), _offset);
            }

            TEST(EntryDeserializerTest, OfferServiceEntry)
            {
                const uint16_t cServiceId{0x0001};
                const uint16_t cInstanceId{0x0002};
                const uint8_t cMajorVersion{0x03};
                const uint32_t cMinorVersion{0x00000004};

                auto _originalEntry{
                    ServiceEntry::CreateOfferServiceEntry(
                        cServiceId, cInstanceId, cMajorVersion, cMinorVersion)};

                uint8_t _optionIndex{0};
                const std::vector<uint8_t> cPayload{_originalEntry->Payload(_optionIndex)};

                std::size_t _offset{0};
                uint8_t _numberOfFirstOptions;
                uint8_t _numberOfSecondOptions;
                auto _deserializedEntry{
                    EntryDeserializer::Deserialize(
                        cPayload, _offset, _numberOfFirstOptions, _numberOfSecondOptions)};

                auto _serviceEntry{dynamic_cast<ServiceEntry *>(_deserializedEntry.get())};
                ASSERT_NE(nullptr, _serviceEntry);

                EXPECT_EQ(EntryType::Offering, _serviceEntry->Type());
                EXPECT_EQ(_originalEntry->TTL(), _serviceEntry->TTL());
                EXPECT_EQ(cMinorVersion, _serviceEntry->MinorVersion());
            }

            TEST(EntryDeserializerTest, SubscribeEventgroupEntry)
            {
                const uint16_t cServiceId{0x0102};
                const uint16_t cInstanceId{0x0304};
                const uint8_t cMajorVersion{0x05};
                const uint8_t cCounter{0x06};
                const uint16_t cEventgroupId{0x0708};

                auto _originalEntry{
                    EventgroupEntry::CreateSubscribeEventEntry(
                        cServiceId, cInstanceId, cMajorVersion, cCounter, cEventgroupId)};

                uint8_t _optionIndex{0};
                const std::vector<uint8_t> cPayload{_originalEntry->Payload(_optionIndex)};

                std::size_t _offset{0};
                uint8_t _numberOfFirstOptions;
                uint8_t _numberOfSecondOptions;
                auto _deserializedEntry{
                    EntryDeserializer::Deserialize(
                        cPayload, _offset, _numberOfFirstOptions, _numberOfSecondOptions)};

                auto _eventgroupEntry{dynamic_cast<EventgroupEntry *>(_deserializedEntry.get())};
                ASSERT_NE(nullptr, _eventgroupEntry);

                EXPECT_EQ(EntryType::Subscribing, _eventgroupEntry->Type());
                EXPECT_EQ(cServiceId, _eventgroupEntry->ServiceId());
                EXPECT_EQ(cInstanceId, _eventgroupEntry->InstanceId());
                EXPECT_EQ(cMajorVersion, _eventgroupEntry->MajorVersion());
                EXPECT_EQ(_originalEntry->TTL(), _eventgroupEntry->TTL());
                EXPECT_EQ(cCounter, _eventgroupEntry->Counter());
                EXPECT_EQ(cEventgroupId, _eventgroupEntry->EventgroupId());
                EXPECT_EQ(cPayload.size(), _offset);
            }

            TEST(EntryDeserializerTest, AcknowledgeEventgroupEntry)
            {
                auto _subscribeEntry{
                    EventgroupEntry::CreateSubscribeEventEntry(0x0001, 0x0002, 0x03, 0x04, 0x0005)};
                auto _originalEntry{
                    EventgroupEntry::CreateAcknowledgeEntry(_subscribeEntry.get())};

                uint8_t _optionIndex{0};
                const std::vector<uint8_t> cPayload{_originalEntry->Payload(_optionIndex)};

                std::size_t _offset{0};
                uint8_t _numberOfFirstOptions;
                uint8_t _numberOfSecondOptions;
                auto _deserializedEntry{
                    EntryDeserializer::Deserialize(
                        cPayload, _offset, _numberOfFirstOptions, _numberOfSecondOptions)};

                auto _eventgroupEntry{dynamic_cast<EventgroupEntry *>(_deserializedEntry.get())};
                ASSERT_NE(nullptr, _eventgroupEntry);

                EXPECT_EQ(EntryType::Acknowledging, _eventgroupEntry->Type());
                EXPECT_EQ(_originalEntry->EventgroupId(), _eventgroupEntry->EventgroupId());
            }

            TEST(EntryDeserializerTest, OptionCounts)
            {
                auto _originalEntry{
                    ServiceEntry::CreateOfferServiceEntry(0x0001, 0x0002, 0x03, 0x00000004)};

                const helper::Ipv4Address cIpAddress(127, 0, 0, 1);
                _originalEntry->AddFirstOption(
                    option::Ipv4EndpointOption::CreateUnitcastEndpoint(
                        false, cIpAddress, option::Layer4ProtocolType::Tcp, 8080));
                _originalEntry->AddFirstOption(
                    option::Ipv4EndpointOption::CreateUnitcastEndpoint(
                        false, cIpAddress, option::Layer4ProtocolType::Udp, 8081));
                _originalEntry->AddSecondOption(
                    std::unique_ptr<option::LoadBalancingOption>(
                        new option::LoadBalancingOption(true, 3, 4)));

                uint8_t _optionIndex{0};
                const std::vector<uint8_t> cPayload{_originalEntry->Payload(_optionIndex)};

                std::size_t _offset{0};
                uint8_t _numberOfFirstOptions;
                uint8_t _numberOfSecondOptions;
                auto _deserializedEntry{
                    EntryDeserializer::Deserialize(
                        cPayload, _offset, _numberOfFirstOptions, _numberOfSecondOptions)};

                EXPECT_EQ(2, _numberOfFirstOptions);
                EXPECT_EQ(1, _numberOfSecondOptions);
                // Options are carried separately in the SD message, not in the entry itself.
                EXPECT_TRUE(_deserializedEntry->FirstOptions().empty());
                EXPECT_TRUE(_deserializedEntry->SecondOptions().empty());
            }

            TEST(EntryDeserializerTest, NonZeroOffset)
            {
                const std::vector<uint8_t> cPrefix{0xde, 0xad, 0xbe, 0xef};
                const uint16_t cServiceId{0x4321};

                auto _originalEntry{
                    ServiceEntry::CreateFindServiceEntry(cServiceId, 0x000001, 0x0001, 0x01, 0x00000001)};

                uint8_t _optionIndex{0};
                std::vector<uint8_t> _payload{cPrefix};
                const std::vector<uint8_t> cEntryPayload{_originalEntry->Payload(_optionIndex)};
                _payload.insert(_payload.end(), cEntryPayload.cbegin(), cEntryPayload.cend());

                std::size_t _offset{cPrefix.size()};
                uint8_t _numberOfFirstOptions;
                uint8_t _numberOfSecondOptions;
                auto _deserializedEntry{
                    EntryDeserializer::Deserialize(
                        _payload, _offset, _numberOfFirstOptions, _numberOfSecondOptions)};

                EXPECT_EQ(cServiceId, _deserializedEntry->ServiceId());
                EXPECT_EQ(_payload.size(), _offset);
            }

            TEST(EntryDeserializerTest, UnsupportedEntryType)
            {
                const uint8_t cUnsupportedEntryType{0x02};
                std::vector<uint8_t> _payload(16, 0x00);
                _payload[0] = cUnsupportedEntryType;

                std::size_t _offset{0};
                uint8_t _numberOfFirstOptions;
                uint8_t _numberOfSecondOptions;

                EXPECT_THROW(
                    EntryDeserializer::Deserialize(
                        _payload, _offset, _numberOfFirstOptions, _numberOfSecondOptions),
                    std::out_of_range);
            }

            TEST(EntryDeserializerTest, TruncatedPayload)
            {
                const std::vector<uint8_t> cPayload{
                    static_cast<uint8_t>(EntryType::Finding), 0x00, 0x00};

                std::size_t _offset{0};
                uint8_t _numberOfFirstOptions;
                uint8_t _numberOfSecondOptions;

                EXPECT_THROW(
                    EntryDeserializer::Deserialize(
                        cPayload, _offset, _numberOfFirstOptions, _numberOfSecondOptions),
                    std::out_of_range);
            }
        }
    }
}
