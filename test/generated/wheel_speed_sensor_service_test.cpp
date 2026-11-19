#include <gtest/gtest.h>
#include <type_traits>
#include "../../src/arxml/arxml_reader.h"
#include "../../src/generated/wheel_speed_sensor_service/wheel_speed_sensor_service_proxy.h"
#include "../../src/generated/wheel_speed_sensor_service/wheel_speed_sensor_service_skeleton.h"
#include "../ara/com/helper/mockup_network_layer.h"

namespace generated
{
    namespace wheelspeedsensorservice
    {
        static_assert(
            std::is_same<WheelSpeed_T, std::uint16_t>::value,
            "WheelSpeed_T should map to the 16-bit Classic base type.");
        static_assert(
            std::is_same<skeleton::events::WheelSpeed::SampleType, WheelSpeed_T>::value,
            "The skeleton event should carry WheelSpeed_T.");
        static_assert(
            std::is_same<proxy::events::WheelSpeed::SampleType, WheelSpeed_T>::value,
            "The proxy event should carry WheelSpeed_T.");

        class WheelSpeedSensorServiceTest : public testing::Test
        {
        protected:
            ara::com::helper::MockupNetworkLayer<EventMessage> NetworkLayer;
            skeleton::WheelSpeedSensorServiceSkeleton Skeleton;
            proxy::WheelSpeedSensorServiceProxy Proxy;

            WheelSpeedSensorServiceTest() : Skeleton(&NetworkLayer),
                                            Proxy(&NetworkLayer)
            {
            }
        };

        TEST(WheelSpeedSensorServiceManifestTest, DeploymentIdentifiers)
        {
            const std::string cManifestFile{
                std::string(GENERATED_SOURCE_DIR) +
                "/wheel_speed_sensor_service/wheel_speed_sensor_service.arxml"};
            const arxml::ArxmlReader cReader(cManifestFile);

            const arxml::ArxmlNode cInterfaceNode{
                cReader.GetRootNode(
                    {"AUTOSAR", "AR-PACKAGES", "AR-PACKAGE", "ELEMENTS",
                     "SERVICE-INTERFACE", "EVENTS", "VARIABLE-DATA-PROTOTYPE"})};
            EXPECT_EQ("WheelSpeed", cInterfaceNode.GetShortName());

            const arxml::ArxmlNode cServiceIdNode{
                cReader.GetRootNode(
                    {"AUTOSAR", "AR-PACKAGES", "AR-PACKAGE", "ELEMENTS",
                     "SOMEIP-SERVICE-INTERFACE-DEPLOYMENT", "SERVICE-INTERFACE-ID"})};
            EXPECT_EQ(cServiceId, cServiceIdNode.GetValue<std::uint16_t>());

            const arxml::ArxmlNode cMajorVersionNode{
                cReader.GetRootNode(
                    {"AUTOSAR", "AR-PACKAGES", "AR-PACKAGE", "ELEMENTS",
                     "SOMEIP-SERVICE-INTERFACE-DEPLOYMENT",
                     "SERVICE-INTERFACE-VERSION", "MAJOR-VERSION"})};
            EXPECT_EQ(cMajorVersion, cMajorVersionNode.GetValue<std::uint8_t>());

            const arxml::ArxmlNode cEventIdNode{
                cReader.GetRootNode(
                    {"AUTOSAR", "AR-PACKAGES", "AR-PACKAGE", "ELEMENTS",
                     "SOMEIP-SERVICE-INTERFACE-DEPLOYMENT", "EVENT-DEPLOYMENTS",
                     "SOMEIP-EVENT-DEPLOYMENT", "EVENT-ID"})};
            EXPECT_EQ(cWheelSpeedEventId, cEventIdNode.GetValue<std::uint16_t>());

            const arxml::ArxmlNode cEventgroupIdNode{
                cReader.GetRootNode(
                    {"AUTOSAR", "AR-PACKAGES", "AR-PACKAGE", "ELEMENTS",
                     "SOMEIP-SERVICE-INTERFACE-DEPLOYMENT", "EVENT-GROUPS",
                     "SOMEIP-EVENT-GROUP", "EVENT-GROUP-ID"})};
            EXPECT_EQ(cEventgroupId, cEventgroupIdNode.GetValue<std::uint16_t>());
        }

        TEST(WheelSpeedSensorServiceMessageTest, NotificationWireFormat)
        {
            const std::uint16_t cSessionId{2};
            const std::vector<std::uint8_t> cExpectedPayload{
                0x10, 0x00, 0x80, 0x01, // Message ID (service ID + event ID)
                0x00, 0x00, 0x00, 0x0a, // Length
                0x00, 0x00, 0x00, 0x02, // Client ID + session ID
                0x01, 0x01, 0x02, 0x00, // Versions, notification, E_OK
                0x12, 0x34};            // WheelSpeed_T in network byte order

            std::vector<std::uint8_t> _eventPayload;
            const WheelSpeed_T cSample{0x1234};
            serialization::Serialize(_eventPayload, cSample);
            const EventMessage cMessage(
                cWheelSpeedEventId, cSessionId, std::move(_eventPayload));

            EXPECT_EQ(cExpectedPayload, cMessage.Payload());

            const EventMessage cDeserialized{
                EventMessage::Deserialize(cExpectedPayload)};
            EXPECT_EQ(cServiceId, cDeserialized.ServiceId());
            EXPECT_EQ(cWheelSpeedEventId, cDeserialized.EventId());
            EXPECT_EQ(cSessionId, cDeserialized.SessionId());
            EXPECT_EQ(
                ara::com::someip::SomeIpMessageType::Notification,
                cDeserialized.MessageType());
            EXPECT_EQ(cMessage.EventPayload(), cDeserialized.EventPayload());
        }

        TEST(WheelSpeedSensorServiceMessageTest, TruncatedHeader)
        {
            const std::vector<std::uint8_t> cTruncatedPayload{0x10, 0x00, 0x80};

            EXPECT_THROW(
                EventMessage::Deserialize(cTruncatedPayload), std::out_of_range);
        }

        TEST_F(WheelSpeedSensorServiceTest, SendWithoutOffer)
        {
            const WheelSpeed_T cSample{1500};
            Proxy.WheelSpeed.Subscribe();

            EXPECT_FALSE(Skeleton.IsOffered());
            EXPECT_FALSE(Skeleton.WheelSpeed.Send(cSample));

            WheelSpeed_T _sample;
            EXPECT_FALSE(Proxy.WheelSpeed.TryGetLastSample(_sample));
        }

        TEST_F(WheelSpeedSensorServiceTest, EventRoundTrip)
        {
            // 123.45 km/h with the Classic 0.01 km/h per LSB scaling
            const WheelSpeed_T cSample{12345};
            std::vector<WheelSpeed_T> _receivedSamples;
            Proxy.WheelSpeed.SetReceiveHandler(
                [&_receivedSamples](const WheelSpeed_T &sample)
                {
                    _receivedSamples.push_back(sample);
                });
            Proxy.WheelSpeed.Subscribe();
            Skeleton.OfferService();

            EXPECT_TRUE(Skeleton.WheelSpeed.Send(cSample));
            EXPECT_TRUE(Skeleton.WheelSpeed.Send(cSample + 1));

            const std::vector<WheelSpeed_T> cExpectedSamples{12345, 12346};
            EXPECT_EQ(cExpectedSamples, _receivedSamples);

            WheelSpeed_T _lastSample;
            EXPECT_TRUE(Proxy.WheelSpeed.TryGetLastSample(_lastSample));
            EXPECT_EQ(cSample + 1, _lastSample);
        }

        TEST_F(WheelSpeedSensorServiceTest, SessionIdIncrement)
        {
            std::vector<std::uint16_t> _sessionIds;
            NetworkLayer.SetReceiver(
                &_sessionIds,
                [&_sessionIds](EventMessage message)
                {
                    _sessionIds.push_back(message.SessionId());
                });
            Skeleton.OfferService();

            Skeleton.WheelSpeed.Send(0);
            Skeleton.WheelSpeed.Send(0);

            const std::vector<std::uint16_t> cExpectedSessionIds{1, 2};
            EXPECT_EQ(cExpectedSessionIds, _sessionIds);
        }

        TEST_F(WheelSpeedSensorServiceTest, NoSampleWithoutSubscription)
        {
            const WheelSpeed_T cSample{42};
            Skeleton.OfferService();

            EXPECT_TRUE(Skeleton.WheelSpeed.Send(cSample));

            WheelSpeed_T _sample;
            EXPECT_FALSE(Proxy.WheelSpeed.IsSubscribed());
            EXPECT_FALSE(Proxy.WheelSpeed.TryGetLastSample(_sample));
        }

        TEST_F(WheelSpeedSensorServiceTest, StopOfferAndUnsubscribe)
        {
            const WheelSpeed_T cSample{42};
            Proxy.WheelSpeed.Subscribe();
            Skeleton.OfferService();
            EXPECT_TRUE(Skeleton.WheelSpeed.Send(cSample));

            Skeleton.StopOfferService();
            EXPECT_FALSE(Skeleton.WheelSpeed.Send(cSample));

            WheelSpeed_T _sample;
            EXPECT_TRUE(Proxy.WheelSpeed.TryGetLastSample(_sample));
            Proxy.WheelSpeed.Unsubscribe();
            EXPECT_FALSE(Proxy.WheelSpeed.TryGetLastSample(_sample));
        }

        TEST_F(WheelSpeedSensorServiceTest, ForeignAndMalformedNotifications)
        {
            const std::uint16_t cForeignEventId{0x8002};
            const std::uint16_t cSessionId{1};
            Proxy.WheelSpeed.Subscribe();

            const EventMessage cForeignMessage(
                cForeignEventId, cSessionId, std::vector<std::uint8_t>{0x00, 0x01});
            EXPECT_FALSE(Proxy.WheelSpeed.Process(cForeignMessage));

            const EventMessage cShortMessage(
                cWheelSpeedEventId, cSessionId, std::vector<std::uint8_t>{0x01});
            EXPECT_FALSE(Proxy.WheelSpeed.Process(cShortMessage));

            WheelSpeed_T _sample;
            EXPECT_FALSE(Proxy.WheelSpeed.TryGetLastSample(_sample));
        }
    }
}
