#include <gtest/gtest.h>
#include <utility>
#include "../../../../src/ara/com/entry/service_entry.h"
#include "../../../../src/ara/com/entry/eventgroup_entry.h"
#include "../../../../src/ara/com/option/ipv4_endpoint_option.h"
#include "../../../../src/ara/com/option/loadbalancing_option.h"

namespace ara
{
    namespace com
    {
        namespace entry
        {
            /// @brief Test stand-in for the configuration option which has no concrete class in the stack
            class ConfigurationOptionStub : public option::Option
            {
            public:
                ConfigurationOptionStub() noexcept : option::Option(option::OptionType::Configuration, false)
                {
                }

                uint16_t Length() const noexcept override
                {
                    return 1;
                }

                std::vector<uint8_t> Payload() const override
                {
                    return BasePayload();
                }
            };

            class EntryTest : public testing::Test
            {
            protected:
                static const uint16_t cServiceId{0x0102};
                static const uint16_t cInstanceId{0x0304};
                static const uint8_t cMajorVersion{0x05};
                static const uint32_t cMinorVersion{0x06070809};
                static const uint32_t cTtl{0x0a0b0c};
                static const uint8_t cCounter{0x03};
                static const uint16_t cEventgroupId{0x0d0e};
                static const uint16_t cPort{8080};

                std::unique_ptr<ServiceEntry> CreateOffer() const
                {
                    return ServiceEntry::CreateOfferServiceEntry(
                        cServiceId, cInstanceId, cMajorVersion, cMinorVersion, cTtl);
                }

                std::unique_ptr<EventgroupEntry> CreateSubscribe() const
                {
                    return EventgroupEntry::CreateSubscribeEventEntry(
                        cServiceId, cInstanceId, cMajorVersion, cCounter, cEventgroupId);
                }

                std::unique_ptr<option::Option> CreateUnicast() const
                {
                    const helper::Ipv4Address cIpAddress(192, 168, 0, 1);
                    return option::Ipv4EndpointOption::CreateUnitcastEndpoint(
                        false, cIpAddress, option::Layer4ProtocolType::Tcp, cPort);
                }

                std::unique_ptr<option::Option> CreateMulticast() const
                {
                    const helper::Ipv4Address cIpAddress(239, 0, 0, 1);
                    return option::Ipv4EndpointOption::CreateMulticastEndpoint(
                        false, cIpAddress, cPort);
                }

                std::unique_ptr<option::Option> CreateSdEndpoint() const
                {
                    const helper::Ipv4Address cIpAddress(192, 168, 0, 1);
                    return option::Ipv4EndpointOption::CreateSdEndpoint(
                        false, cIpAddress);
                }

                std::unique_ptr<option::Option> CreateLoadBalancing() const
                {
                    return std::unique_ptr<option::Option>(
                        new option::LoadBalancingOption(false, 1, 2));
                }

                std::unique_ptr<option::Option> CreateConfiguration() const
                {
                    return std::unique_ptr<option::Option>(
                        new ConfigurationOptionStub());
                }
            };

            const uint16_t EntryTest::cServiceId;
            const uint16_t EntryTest::cInstanceId;
            const uint8_t EntryTest::cMajorVersion;
            const uint32_t EntryTest::cMinorVersion;
            const uint32_t EntryTest::cTtl;
            const uint8_t EntryTest::cCounter;
            const uint16_t EntryTest::cEventgroupId;
            const uint16_t EntryTest::cPort;

            TEST_F(EntryTest, FindServiceEntryZeroTtl)
            {
                EXPECT_THROW(
                    ServiceEntry::CreateFindServiceEntry(cServiceId, 0),
                    std::invalid_argument);
            }

            TEST_F(EntryTest, OfferServiceEntryZeroTtl)
            {
                EXPECT_THROW(
                    ServiceEntry::CreateOfferServiceEntry(
                        cServiceId, cInstanceId, cMajorVersion, cMinorVersion, 0),
                    std::invalid_argument);
            }

            TEST_F(EntryTest, StopOfferEntryPayload)
            {
                const std::vector<uint8_t> cExpectedPayload{
                    0x01, 0x00, 0x00, 0x00,
                    0x01, 0x02, 0x03, 0x04,
                    0x05, 0x00, 0x00, 0x00,
                    0x06, 0x07, 0x08, 0x09};

                std::unique_ptr<ServiceEntry> _entry{
                    ServiceEntry::CreateStopOfferEntry(
                        cServiceId, cInstanceId, cMajorVersion, cMinorVersion)};
                uint8_t _optionIndex{0};

                EXPECT_EQ(EntryType::Offering, _entry->Type());
                EXPECT_EQ(0U, _entry->TTL());
                EXPECT_EQ(cExpectedPayload, _entry->Payload(_optionIndex));
            }

            TEST_F(EntryTest, ServiceEntryMoveConstructor)
            {
                std::unique_ptr<ServiceEntry> _entry{CreateOffer()};
                _entry->AddFirstOption(CreateUnicast());
                _entry->AddSecondOption(CreateLoadBalancing());

                ServiceEntry _movedEntry{std::move(*_entry)};

                EXPECT_EQ(EntryType::Offering, _movedEntry.Type());
                EXPECT_EQ(cServiceId, _movedEntry.ServiceId());
                EXPECT_EQ(cInstanceId, _movedEntry.InstanceId());
                EXPECT_EQ(cMajorVersion, _movedEntry.MajorVersion());
                EXPECT_EQ(cMinorVersion, _movedEntry.MinorVersion());
                EXPECT_EQ(cTtl, _movedEntry.TTL());
                EXPECT_EQ(1U, _movedEntry.FirstOptions().size());
                EXPECT_EQ(1U, _movedEntry.SecondOptions().size());
            }

            TEST_F(EntryTest, ServiceEntryMoveAssignment)
            {
                std::unique_ptr<ServiceEntry> _entry{CreateOffer()};
                _entry->AddFirstOption(CreateUnicast());
                std::unique_ptr<ServiceEntry> _otherEntry{
                    ServiceEntry::CreateFindServiceEntry(0xffff)};

                *_otherEntry = std::move(*_entry);

                EXPECT_EQ(EntryType::Offering, _otherEntry->Type());
                EXPECT_EQ(cServiceId, _otherEntry->ServiceId());
                EXPECT_EQ(cInstanceId, _otherEntry->InstanceId());
                EXPECT_EQ(cMajorVersion, _otherEntry->MajorVersion());
                EXPECT_EQ(cMinorVersion, _otherEntry->MinorVersion());
                EXPECT_EQ(cTtl, _otherEntry->TTL());
                EXPECT_EQ(1U, _otherEntry->FirstOptions().size());
                EXPECT_TRUE(_otherEntry->SecondOptions().empty());
            }

            TEST_F(EntryTest, ServiceEntryRejectsMulticastOption)
            {
                std::unique_ptr<ServiceEntry> _entry{CreateOffer()};

                EXPECT_THROW(
                    _entry->AddFirstOption(CreateMulticast()),
                    std::invalid_argument);
                EXPECT_THROW(
                    _entry->AddSecondOption(CreateMulticast()),
                    std::invalid_argument);
                EXPECT_TRUE(_entry->FirstOptions().empty());
                EXPECT_TRUE(_entry->SecondOptions().empty());
            }

            TEST_F(EntryTest, ServiceEntryRejectsSdEndpointOption)
            {
                std::unique_ptr<ServiceEntry> _entry{CreateOffer()};

                EXPECT_THROW(
                    _entry->AddFirstOption(CreateSdEndpoint()),
                    std::invalid_argument);
            }

            TEST_F(EntryTest, SingleLoadBalancingOptionInFirstOptions)
            {
                std::unique_ptr<ServiceEntry> _entry{CreateOffer()};

                EXPECT_NO_THROW(_entry->AddFirstOption(CreateLoadBalancing()));
                EXPECT_THROW(
                    _entry->AddFirstOption(CreateLoadBalancing()),
                    std::invalid_argument);
                EXPECT_THROW(
                    _entry->AddSecondOption(CreateLoadBalancing()),
                    std::invalid_argument);
                EXPECT_EQ(1U, _entry->FirstOptions().size());
            }

            TEST_F(EntryTest, SingleLoadBalancingOptionInSecondOptions)
            {
                std::unique_ptr<ServiceEntry> _entry{CreateOffer()};
                _entry->AddFirstOption(CreateUnicast());

                EXPECT_NO_THROW(_entry->AddSecondOption(CreateLoadBalancing()));
                EXPECT_THROW(
                    _entry->AddFirstOption(CreateLoadBalancing()),
                    std::invalid_argument);
                EXPECT_EQ(1U, _entry->SecondOptions().size());
            }

            TEST_F(EntryTest, SingleConfigurationOption)
            {
                std::unique_ptr<ServiceEntry> _entry{CreateOffer()};

                EXPECT_NO_THROW(_entry->AddFirstOption(CreateConfiguration()));
                EXPECT_THROW(
                    _entry->AddSecondOption(CreateConfiguration()),
                    std::invalid_argument);
                // A different option type is still accepted
                EXPECT_NO_THROW(_entry->AddSecondOption(CreateLoadBalancing()));
            }

            TEST_F(EntryTest, PayloadOptionIndexing)
            {
                const uint8_t cInitialOptionIndex{3};
                const uint8_t cExpectedOptionIndex{6};
                const std::vector<uint8_t> cExpectedPayload{
                    0x01, 0x03, 0x05, 0x21,
                    0x01, 0x02, 0x03, 0x04,
                    0x05, 0x0a, 0x0b, 0x0c,
                    0x06, 0x07, 0x08, 0x09};

                std::unique_ptr<ServiceEntry> _entry{CreateOffer()};
                _entry->AddFirstOption(CreateUnicast());
                _entry->AddFirstOption(CreateUnicast());
                _entry->AddSecondOption(CreateLoadBalancing());
                uint8_t _optionIndex{cInitialOptionIndex};

                EXPECT_EQ(cExpectedPayload, _entry->Payload(_optionIndex));
                EXPECT_EQ(cExpectedOptionIndex, _optionIndex);
            }

            TEST_F(EntryTest, ServiceEntryDeserializeFinding)
            {
                const std::vector<uint8_t> cPayload{0x06, 0x07, 0x08, 0x09};
                const std::size_t cExpectedOffset{4};
                std::size_t _offset{0};

                std::unique_ptr<ServiceEntry> _entry{
                    ServiceEntry::Deserialize(
                        cPayload,
                        _offset,
                        EntryType::Finding,
                        cServiceId,
                        cInstanceId,
                        cTtl,
                        cMajorVersion)};

                EXPECT_EQ(cExpectedOffset, _offset);
                EXPECT_EQ(EntryType::Finding, _entry->Type());
                EXPECT_EQ(cServiceId, _entry->ServiceId());
                EXPECT_EQ(cInstanceId, _entry->InstanceId());
                EXPECT_EQ(cMajorVersion, _entry->MajorVersion());
                EXPECT_EQ(cMinorVersion, _entry->MinorVersion());
                EXPECT_EQ(cTtl, _entry->TTL());
            }

            TEST_F(EntryTest, ServiceEntryDeserializeStopOffer)
            {
                const std::vector<uint8_t> cPayload{0x06, 0x07, 0x08, 0x09};
                std::size_t _offset{0};

                std::unique_ptr<ServiceEntry> _entry{
                    ServiceEntry::Deserialize(
                        cPayload,
                        _offset,
                        EntryType::Offering,
                        cServiceId,
                        cInstanceId,
                        0,
                        cMajorVersion)};

                EXPECT_EQ(EntryType::Offering, _entry->Type());
                EXPECT_EQ(cMinorVersion, _entry->MinorVersion());
                EXPECT_EQ(0U, _entry->TTL());
            }

            TEST_F(EntryTest, ServiceEntryDeserializeInvalidType)
            {
                const std::vector<uint8_t> cPayload{0x06, 0x07, 0x08, 0x09};
                std::size_t _offset{0};

                EXPECT_THROW(
                    ServiceEntry::Deserialize(
                        cPayload,
                        _offset,
                        EntryType::Subscribing,
                        cServiceId,
                        cInstanceId,
                        cTtl,
                        cMajorVersion),
                    std::out_of_range);
            }

            TEST_F(EntryTest, EventgroupEntryCounterOutOfRange)
            {
                const uint8_t cInvalidCounter{0x10};

                EXPECT_THROW(
                    EventgroupEntry::CreateSubscribeEventEntry(
                        cServiceId, cInstanceId, cMajorVersion, cInvalidCounter, cEventgroupId),
                    std::out_of_range);
                EXPECT_THROW(
                    EventgroupEntry::CreateUnsubscribeEventEntry(
                        cServiceId, cInstanceId, cMajorVersion, cInvalidCounter, cEventgroupId),
                    std::out_of_range);
            }

            TEST_F(EntryTest, EventgroupEntryMoveConstructor)
            {
                std::unique_ptr<EventgroupEntry> _entry{CreateSubscribe()};
                _entry->AddFirstOption(CreateUnicast());

                EventgroupEntry _movedEntry{std::move(*_entry)};

                EXPECT_EQ(EntryType::Subscribing, _movedEntry.Type());
                EXPECT_EQ(cServiceId, _movedEntry.ServiceId());
                EXPECT_EQ(cInstanceId, _movedEntry.InstanceId());
                EXPECT_EQ(cMajorVersion, _movedEntry.MajorVersion());
                EXPECT_EQ(cCounter, _movedEntry.Counter());
                EXPECT_EQ(cEventgroupId, _movedEntry.EventgroupId());
                EXPECT_EQ(1U, _movedEntry.FirstOptions().size());
            }

            TEST_F(EntryTest, EventgroupEntryMoveAssignment)
            {
                std::unique_ptr<EventgroupEntry> _entry{CreateSubscribe()};
                _entry->AddFirstOption(CreateUnicast());
                std::unique_ptr<EventgroupEntry> _otherEntry{
                    EventgroupEntry::CreateUnsubscribeEventEntry(0, 0, 0, 0, 0)};

                *_otherEntry = std::move(*_entry);

                EXPECT_EQ(cServiceId, _otherEntry->ServiceId());
                EXPECT_EQ(cInstanceId, _otherEntry->InstanceId());
                EXPECT_EQ(cMajorVersion, _otherEntry->MajorVersion());
                EXPECT_EQ(cCounter, _otherEntry->Counter());
                EXPECT_EQ(cEventgroupId, _otherEntry->EventgroupId());
                EXPECT_GT(_otherEntry->TTL(), 0U);
                EXPECT_EQ(1U, _otherEntry->FirstOptions().size());
            }

            TEST_F(EntryTest, UnsubscribeEventEntryPayload)
            {
                const std::vector<uint8_t> cExpectedPayload{
                    0x06, 0x00, 0x00, 0x00,
                    0x01, 0x02, 0x03, 0x04,
                    0x05, 0x00, 0x00, 0x00,
                    0x00, 0x03, 0x0d, 0x0e};

                std::unique_ptr<EventgroupEntry> _entry{
                    EventgroupEntry::CreateUnsubscribeEventEntry(
                        cServiceId, cInstanceId, cMajorVersion, cCounter, cEventgroupId)};
                uint8_t _optionIndex{0};

                EXPECT_EQ(EntryType::Subscribing, _entry->Type());
                EXPECT_EQ(0U, _entry->TTL());
                EXPECT_EQ(cExpectedPayload, _entry->Payload(_optionIndex));
            }

            TEST_F(EntryTest, SubscribeEntryOptionValidation)
            {
                std::unique_ptr<EventgroupEntry> _entry{CreateSubscribe()};

                EXPECT_NO_THROW(_entry->AddFirstOption(CreateUnicast()));
                EXPECT_THROW(
                    _entry->AddFirstOption(CreateMulticast()),
                    std::invalid_argument);
                EXPECT_THROW(
                    _entry->AddFirstOption(CreateSdEndpoint()),
                    std::invalid_argument);
                // Current behaviour: only endpoint options pass the event-group validation.
                EXPECT_THROW(
                    _entry->AddSecondOption(CreateLoadBalancing()),
                    std::invalid_argument);
            }

            TEST_F(EntryTest, AcknowledgeEntryOptionValidation)
            {
                std::unique_ptr<EventgroupEntry> _subscribeEntry{CreateSubscribe()};
                std::unique_ptr<EventgroupEntry> _entry{
                    EventgroupEntry::CreateAcknowledgeEntry(_subscribeEntry.get())};

                EXPECT_EQ(EntryType::Acknowledging, _entry->Type());
                EXPECT_EQ(_subscribeEntry->TTL(), _entry->TTL());
                EXPECT_THROW(
                    _entry->AddFirstOption(CreateUnicast()),
                    std::invalid_argument);
                EXPECT_NO_THROW(_entry->AddFirstOption(CreateMulticast()));
                // Only one multicast endpoint is allowed per acknowledgement
                EXPECT_THROW(
                    _entry->AddSecondOption(CreateMulticast()),
                    std::invalid_argument);
                EXPECT_EQ(1U, _entry->FirstOptions().size());
            }

            TEST_F(EntryTest, NegativeAcknowledgeEntryOptionValidation)
            {
                std::unique_ptr<EventgroupEntry> _subscribeEntry{CreateSubscribe()};
                std::unique_ptr<EventgroupEntry> _entry{
                    EventgroupEntry::CreateNegativeAcknowledgeEntry(
                        _subscribeEntry.get())};

                EXPECT_EQ(EntryType::Acknowledging, _entry->Type());
                EXPECT_EQ(0U, _entry->TTL());
                EXPECT_EQ(cCounter, _entry->Counter());
                EXPECT_EQ(cEventgroupId, _entry->EventgroupId());
                EXPECT_THROW(
                    _entry->AddFirstOption(CreateMulticast()),
                    std::invalid_argument);
            }

            TEST_F(EntryTest, EventgroupEntryDeserializeUnsubscribe)
            {
                const std::vector<uint8_t> cPayload{0x00, 0x03, 0x0d, 0x0e};
                const std::size_t cExpectedOffset{4};
                std::size_t _offset{0};

                std::unique_ptr<EventgroupEntry> _entry{
                    EventgroupEntry::Deserialize(
                        cPayload,
                        _offset,
                        EntryType::Subscribing,
                        cServiceId,
                        cInstanceId,
                        0,
                        cMajorVersion)};

                EXPECT_EQ(cExpectedOffset, _offset);
                EXPECT_EQ(EntryType::Subscribing, _entry->Type());
                EXPECT_EQ(0U, _entry->TTL());
                EXPECT_EQ(cCounter, _entry->Counter());
                EXPECT_EQ(cEventgroupId, _entry->EventgroupId());
            }

            TEST_F(EntryTest, EventgroupEntryDeserializeAcknowledge)
            {
                const std::vector<uint8_t> cPayload{0x00, 0x03, 0x0d, 0x0e};
                std::size_t _offset{0};

                std::unique_ptr<EventgroupEntry> _entry{
                    EventgroupEntry::Deserialize(
                        cPayload,
                        _offset,
                        EntryType::Acknowledging,
                        cServiceId,
                        cInstanceId,
                        cTtl,
                        cMajorVersion)};

                EXPECT_EQ(EntryType::Acknowledging, _entry->Type());
                EXPECT_EQ(cTtl, _entry->TTL());
                EXPECT_EQ(cCounter, _entry->Counter());
                EXPECT_EQ(cEventgroupId, _entry->EventgroupId());
            }

            TEST_F(EntryTest, EventgroupEntryDeserializeInvalidType)
            {
                const std::vector<uint8_t> cPayload{0x00, 0x03, 0x0d, 0x0e};
                std::size_t _offset{0};

                EXPECT_THROW(
                    EventgroupEntry::Deserialize(
                        cPayload,
                        _offset,
                        EntryType::Offering,
                        cServiceId,
                        cInstanceId,
                        cTtl,
                        cMajorVersion),
                    std::out_of_range);
            }

            TEST_F(EntryTest, EventgroupEntryDeserializeInvalidCounter)
            {
                const std::vector<uint8_t> cPayload{0x00, 0x10, 0x0d, 0x0e};
                std::size_t _offset{0};

                EXPECT_THROW(
                    EventgroupEntry::Deserialize(
                        cPayload,
                        _offset,
                        EntryType::Subscribing,
                        cServiceId,
                        cInstanceId,
                        cTtl,
                        cMajorVersion),
                    std::out_of_range);
            }
        }
    }
}
