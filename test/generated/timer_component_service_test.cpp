#include <gtest/gtest.h>
#include <type_traits>
#include "../../src/arxml/arxml_reader.h"
#include "../../src/generated/timer_component_service/timer_component_service_proxy.h"
#include "../../src/generated/timer_component_service/timer_component_service_skeleton.h"
#include "./loopback_rpc.h"

namespace generated
{
    namespace timercomponentservice
    {
        static_assert(
            std::is_same<uint32, std::uint32_t>::value,
            "The Classic uint32 platform type should map to std::uint32_t.");
        static_assert(
            std::is_same<boolean, bool>::value,
            "The Classic boolean platform type should map to bool.");

        /// @brief Hand-written service implementation on top of the generated skeleton
        class TestTimer : public skeleton::TimerComponentServiceSkeleton
        {
        public:
            uint32 Now;
            bool Healthy;

            explicit TestTimer(
                ara::com::someip::rpc::RpcServer *rpcServer) : skeleton::TimerComponentServiceSkeleton(rpcServer),
                                                               Now{0},
                                                               Healthy{true}
            {
            }

            bool GetTime(uint32 &value) override
            {
                value = Now;
                return Healthy;
            }

            bool IsTimerElapsed(
                uint32 startTime, uint32 duration, boolean &result) override
            {
                result = (Now - startTime) >= duration;
                return Healthy;
            }
        };

        class TimerComponentServiceTest : public testing::Test
        {
        private:
            static const std::uint16_t cClientId{7};

        protected:
            LoopbackRpcServer RpcServer;
            LoopbackRpcClient RpcClient;
            TestTimer Skeleton;
            proxy::TimerComponentServiceProxy Proxy;

            TimerComponentServiceTest() : RpcServer(cProtocolVersion, cMajorVersion),
                                          RpcClient(&RpcServer, cProtocolVersion, cMajorVersion),
                                          Skeleton(&RpcServer),
                                          Proxy(&RpcClient, cClientId)
            {
            }
        };

        TEST(TimerComponentServiceManifestTest, DeploymentIdentifiers)
        {
            const std::string cManifestFile{
                std::string(GENERATED_SOURCE_DIR) +
                "/timer_component_service/timer_component_service.arxml"};
            const arxml::ArxmlReader cReader(cManifestFile);

            const arxml::ArxmlNode cServiceIdNode{
                cReader.GetRootNode(
                    {"AUTOSAR", "AR-PACKAGES", "AR-PACKAGE", "ELEMENTS",
                     "SOMEIP-SERVICE-INTERFACE-DEPLOYMENT", "SERVICE-INTERFACE-ID"})};
            EXPECT_EQ(cServiceId, cServiceIdNode.GetValue<std::uint16_t>());

            // The first method deployment is GetTime
            const arxml::ArxmlNode cMethodIdNode{
                cReader.GetRootNode(
                    {"AUTOSAR", "AR-PACKAGES", "AR-PACKAGE", "ELEMENTS",
                     "SOMEIP-SERVICE-INTERFACE-DEPLOYMENT", "METHOD-DEPLOYMENTS",
                     "SOMEIP-METHOD-DEPLOYMENT", "METHOD-ID"})};
            EXPECT_EQ(cGetTimeMethodId, cMethodIdNode.GetValue<std::uint16_t>());

            const arxml::ArxmlNodeRange cMethodNodes{
                cReader.GetNodes(
                    {"AUTOSAR", "AR-PACKAGES", "AR-PACKAGE", "ELEMENTS",
                     "SOMEIP-SERVICE-INTERFACE-DEPLOYMENT", "METHOD-DEPLOYMENTS"})};
            std::vector<std::string> _methodNames;
            for (const arxml::ArxmlNode node : cMethodNodes)
            {
                _methodNames.push_back(node.GetShortName());
            }

            const std::vector<std::string> cExpectedMethodNames{
                "GetTime", "IsTimerElapsed"};
            EXPECT_EQ(cExpectedMethodNames, _methodNames);
        }

        TEST_F(TimerComponentServiceTest, GetTimeMethod)
        {
            const uint32 cNow{0x01020304};
            Skeleton.Now = cNow;
            Skeleton.OfferService();

            bool _successful{false};
            uint32 _value{0};
            Proxy.GetTime(
                [&_successful, &_value](bool successful, uint32 value)
                {
                    _successful = successful;
                    _value = value;
                });

            EXPECT_TRUE(_successful);
            EXPECT_EQ(cNow, _value);
        }

        TEST_F(TimerComponentServiceTest, IsTimerElapsedMethod)
        {
            const uint32 cStartTime{1000};
            const uint32 cDuration{500};
            Skeleton.OfferService();

            bool _successful{false};
            boolean _result{true};
            proxy::TimerComponentServiceProxy::IsTimerElapsedHandler _handler{
                [&_successful, &_result](bool successful, boolean result)
                {
                    _successful = successful;
                    _result = result;
                }};

            Skeleton.Now = cStartTime + cDuration - 1;
            Proxy.IsTimerElapsed(cStartTime, cDuration, _handler);
            EXPECT_TRUE(_successful);
            EXPECT_FALSE(_result);

            Skeleton.Now = cStartTime + cDuration;
            Proxy.IsTimerElapsed(cStartTime, cDuration, _handler);
            EXPECT_TRUE(_successful);
            EXPECT_TRUE(_result);
        }

        TEST_F(TimerComponentServiceTest, MethodWithoutOffer)
        {
            bool _invoked{false};
            bool _successful{true};
            Proxy.GetTime(
                [&_invoked, &_successful](bool successful, uint32)
                {
                    _invoked = true;
                    _successful = successful;
                });

            EXPECT_TRUE(_invoked);
            EXPECT_FALSE(_successful);
        }

        TEST_F(TimerComponentServiceTest, MethodFailure)
        {
            Skeleton.Healthy = false;
            Skeleton.OfferService();

            bool _invoked{false};
            bool _successful{true};
            Proxy.GetTime(
                [&_invoked, &_successful](bool successful, uint32)
                {
                    _invoked = true;
                    _successful = successful;
                });

            EXPECT_TRUE(_invoked);
            EXPECT_FALSE(_successful);
        }

        TEST(TimerComponentServiceLifetimeTest, MethodAfterSkeletonDestruction)
        {
            const std::uint16_t cClientId{7};
            LoopbackRpcServer _rpcServer(cProtocolVersion, cMajorVersion);
            LoopbackRpcClient _rpcClient(
                &_rpcServer, cProtocolVersion, cMajorVersion);
            proxy::TimerComponentServiceProxy _proxy(&_rpcClient, cClientId);

            {
                TestTimer _skeleton(&_rpcServer);
                _skeleton.OfferService();
            }

            bool _invoked{false};
            bool _successful{true};
            _proxy.GetTime(
                [&_invoked, &_successful](bool successful, uint32)
                {
                    _invoked = true;
                    _successful = successful;
                });

            EXPECT_TRUE(_invoked);
            EXPECT_FALSE(_successful);
        }

        TEST(TimerComponentServiceSerializationTest, BasicTypesRoundTrip)
        {
            const std::int16_t cSigned{-2};
            const std::uint64_t cWide{0x0102030405060708};
            const float cFloat{1.5f};
            const std::vector<std::uint8_t> cExpectedPayload{
                0xff, 0xfe,
                0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                0x3f, 0xc0, 0x00, 0x00,
                0x01};

            std::vector<std::uint8_t> _payload;
            serialization::Serialize(_payload, cSigned);
            serialization::Serialize(_payload, cWide);
            serialization::Serialize(_payload, cFloat);
            serialization::Serialize(_payload, true);
            EXPECT_EQ(cExpectedPayload, _payload);

            std::size_t _offset{0};
            std::int16_t _signed{0};
            std::uint64_t _wide{0};
            float _float{0};
            bool _flag{false};
            EXPECT_TRUE(serialization::TryDeserialize(_payload, _offset, _signed));
            EXPECT_TRUE(serialization::TryDeserialize(_payload, _offset, _wide));
            EXPECT_TRUE(serialization::TryDeserialize(_payload, _offset, _float));
            EXPECT_TRUE(serialization::TryDeserialize(_payload, _offset, _flag));
            EXPECT_EQ(cSigned, _signed);
            EXPECT_EQ(cWide, _wide);
            EXPECT_EQ(cFloat, _float);
            EXPECT_TRUE(_flag);
            EXPECT_EQ(_payload.size(), _offset);

            // Nothing is left to extract
            EXPECT_FALSE(serialization::TryDeserialize(_payload, _offset, _flag));
        }
    }
}
