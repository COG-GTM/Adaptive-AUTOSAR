#include <gtest/gtest.h>
#include "../../../src/application/helper/network_configuration.h"
#include "./testable_fixture.h"

namespace application
{
    namespace helper
    {
        namespace
        {
            const char cMinimalManifest[] =
                "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                "<AUTOSAR><AR-PACKAGES><AR-PACKAGE><ELEMENTS>"
                "<COMMUNICATION-CLUSTER><ETHERNET-PHYSICAL-CHANNEL><NETWORK-ENDPOINTS>"
                "<NETWORK-ENDPOINT><SHORT-NAME>FirstEP</SHORT-NAME>"
                "<NETWORK-ENDPOINT-ADDRESSES><IPV-4-CONFIGURATION>"
                "<IPV-4-ADDRESS>10.0.0.1</IPV-4-ADDRESS>"
                "</IPV-4-CONFIGURATION></NETWORK-ENDPOINT-ADDRESSES></NETWORK-ENDPOINT>"
                "<NETWORK-ENDPOINT><SHORT-NAME>SecondEP</SHORT-NAME>"
                "<NETWORK-ENDPOINT-ADDRESSES><IPV-4-CONFIGURATION>"
                "<IPV-4-ADDRESS>10.0.0.2</IPV-4-ADDRESS>"
                "</IPV-4-CONFIGURATION></NETWORK-ENDPOINT-ADDRESSES></NETWORK-ENDPOINT>"
                "</NETWORK-ENDPOINTS></ETHERNET-PHYSICAL-CHANNEL></COMMUNICATION-CLUSTER>"
                "<ETHERNET-COMMUNICATION-CONNECTOR><AP-APPLICATION-ENDPOINTS>"
                "<AP-APPLICATION-ENDPOINT><SHORT-NAME>TcpEndpoint</SHORT-NAME>"
                "<TP-CONFIGURATION><TCP-TP><TCP-TP-PORT><PORT-NUMBER>1234</PORT-NUMBER>"
                "</TCP-TP-PORT></TCP-TP></TP-CONFIGURATION></AP-APPLICATION-ENDPOINT>"
                "<AP-APPLICATION-ENDPOINT><SHORT-NAME>UdpEndpoint</SHORT-NAME>"
                "<TP-CONFIGURATION><UDP-TP><UDP-TP-PORT><PORT-NUMBER>4321</PORT-NUMBER>"
                "</UDP-TP-PORT></UDP-TP></TP-CONFIGURATION></AP-APPLICATION-ENDPOINT>"
                "</AP-APPLICATION-ENDPOINTS></ETHERNET-COMMUNICATION-CONNECTOR>"
                "</ELEMENTS></AR-PACKAGE></AR-PACKAGES></AUTOSAR>";
        }

        TEST(NetworkConfigurationTest, ExecutionManifestTcpEndpoint)
        {
            NetworkConfiguration _configuration;
            EXPECT_TRUE(
                TryGetNetworkConfiguration(
                    fixture::GetConfigurationPath("execution_manifest.arxml"),
                    "RpcServerEP",
                    "ServerUnicastTcp",
                    ara::com::option::Layer4ProtocolType::Tcp,
                    _configuration));
            EXPECT_EQ("127.0.0.1", _configuration.ipAddress);
            EXPECT_EQ(8080, _configuration.portNumber);
        }

        TEST(NetworkConfigurationTest, ExtendedVehicleManifestTcpEndpoint)
        {
            NetworkConfiguration _configuration;
            EXPECT_TRUE(
                TryGetNetworkConfiguration(
                    fixture::GetConfigurationPath("extended_vehicle_manifest.arxml"),
                    "ExtendedVehicleEP",
                    "ServerUnicastTcp",
                    ara::com::option::Layer4ProtocolType::Tcp,
                    _configuration));
            EXPECT_EQ("127.0.0.1", _configuration.ipAddress);
            EXPECT_EQ(8081, _configuration.portNumber);
        }

        TEST(NetworkConfigurationTest, DiagnosticManagerManifestUdpEndpoint)
        {
            NetworkConfiguration _configuration;
            EXPECT_TRUE(
                TryGetNetworkConfiguration(
                    fixture::GetConfigurationPath("diagnostic_manager_manifest.arxml"),
                    "DiagnosticManagerEP",
                    "MulticastUdp",
                    ara::com::option::Layer4ProtocolType::Udp,
                    _configuration));
            EXPECT_EQ("239.0.0.1", _configuration.ipAddress);
            EXPECT_EQ(5555, _configuration.portNumber);
        }

        TEST(NetworkConfigurationTest, InMemoryManifestFilters)
        {
            const arxml::ArxmlReader cReader(cMinimalManifest, sizeof(cMinimalManifest) - 1);
            NetworkConfiguration _configuration;

            EXPECT_TRUE(
                TryGetNetworkConfiguration(
                    cReader, "SecondEP", "TcpEndpoint",
                    ara::com::option::Layer4ProtocolType::Tcp, _configuration));
            EXPECT_EQ("10.0.0.2", _configuration.ipAddress);
            EXPECT_EQ(1234, _configuration.portNumber);

            EXPECT_TRUE(
                TryGetNetworkConfiguration(
                    cReader, "FirstEP", "UdpEndpoint",
                    ara::com::option::Layer4ProtocolType::Udp, _configuration));
            EXPECT_EQ("10.0.0.1", _configuration.ipAddress);
            EXPECT_EQ(4321, _configuration.portNumber);
        }

        TEST(NetworkConfigurationTest, UnknownEndpoints)
        {
            const arxml::ArxmlReader cReader(cMinimalManifest, sizeof(cMinimalManifest) - 1);
            NetworkConfiguration _configuration{"unchanged", 1};

            EXPECT_FALSE(
                TryGetNetworkConfiguration(
                    cReader, "UnknownEP", "TcpEndpoint",
                    ara::com::option::Layer4ProtocolType::Tcp, _configuration));
            EXPECT_FALSE(
                TryGetNetworkConfiguration(
                    cReader, "FirstEP", "UnknownEndpoint",
                    ara::com::option::Layer4ProtocolType::Tcp, _configuration));
            EXPECT_EQ("unchanged", _configuration.ipAddress);
            EXPECT_EQ(1, _configuration.portNumber);
        }

        TEST(NetworkConfigurationTest, UnsupportedProtocol)
        {
            const arxml::ArxmlReader cReader(cMinimalManifest, sizeof(cMinimalManifest) - 1);
            const auto cUnsupportedProtocol{
                static_cast<ara::com::option::Layer4ProtocolType>(0)};
            NetworkConfiguration _configuration;

            EXPECT_FALSE(
                TryGetNetworkConfiguration(
                    cReader, "FirstEP", "TcpEndpoint",
                    cUnsupportedProtocol, _configuration));
        }

        TEST(NetworkConfigurationTest, EmptyFilterMatchesFirstNode)
        {
            const arxml::ArxmlReader cReader(cMinimalManifest, sizeof(cMinimalManifest) - 1);
            std::string _value{"unchanged"};

            // Without a filter, the value of the first shallow node itself is extracted
            EXPECT_TRUE(
                TryExtractDeepValue(
                    cReader,
                    cIpAddressShallowChildren,
                    cIpAddressDeepChildren,
                    "",
                    _value));
            EXPECT_NE("unchanged", _value);
        }

        TEST(NetworkConfigurationTest, MissingManifestFile)
        {
            NetworkConfiguration _configuration;
            EXPECT_THROW(
                TryGetNetworkConfiguration(
                    "/nonexistent/manifest.arxml", "RpcServerEP", "ServerUnicastTcp",
                    ara::com::option::Layer4ProtocolType::Tcp, _configuration),
                std::invalid_argument);
        }
    }
}
