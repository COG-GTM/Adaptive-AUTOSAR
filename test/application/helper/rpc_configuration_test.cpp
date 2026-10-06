#include <gtest/gtest.h>
#include "../../../src/application/helper/rpc_configuration.h"
#include "./testable_fixture.h"

namespace application
{
    namespace helper
    {
        TEST(RpcConfigurationTest, ExecutionManifest)
        {
            RpcConfiguration _configuration;
            EXPECT_TRUE(
                TryGetRpcConfiguration(
                    fixture::GetConfigurationPath("execution_manifest.arxml"),
                    "RpcServerEP",
                    "ServerUnicastTcp",
                    _configuration));
            EXPECT_EQ("127.0.0.1", _configuration.ipAddress);
            EXPECT_EQ(8080, _configuration.portNumber);
            EXPECT_EQ(1, _configuration.protocolVersion);
        }

        TEST(RpcConfigurationTest, ModifiedManifest)
        {
            fixture::TemporaryDirectory _directory;
            const std::string cManifest{
                _directory.WriteManifest(
                    "execution_manifest.arxml",
                    {{"<PORT-NUMBER>8080</PORT-NUMBER>", "<PORT-NUMBER>9090</PORT-NUMBER>"},
                     {"<PROTOCOL-VERSION>1</PROTOCOL-VERSION>", "<PROTOCOL-VERSION>3</PROTOCOL-VERSION>"}})};

            RpcConfiguration _configuration;
            EXPECT_TRUE(
                TryGetRpcConfiguration(
                    cManifest, "RpcServerEP", "ServerUnicastTcp", _configuration));
            EXPECT_EQ(9090, _configuration.portNumber);
            EXPECT_EQ(3, _configuration.protocolVersion);
        }

        TEST(RpcConfigurationTest, MissingProtocolVersion)
        {
            RpcConfiguration _configuration;
            EXPECT_FALSE(
                TryGetRpcConfiguration(
                    fixture::GetConfigurationPath("extended_vehicle_manifest.arxml"),
                    "ExtendedVehicleEP",
                    "ServerUnicastTcp",
                    _configuration));
        }

        TEST(RpcConfigurationTest, UnknownEndpoint)
        {
            RpcConfiguration _configuration;
            EXPECT_FALSE(
                TryGetRpcConfiguration(
                    fixture::GetConfigurationPath("execution_manifest.arxml"),
                    "UnknownEP",
                    "ServerUnicastTcp",
                    _configuration));
            EXPECT_FALSE(
                TryGetRpcConfiguration(
                    fixture::GetConfigurationPath("execution_manifest.arxml"),
                    "RpcServerEP",
                    "UnknownEndpoint",
                    _configuration));
        }

        TEST(RpcConfigurationTest, MissingManifestFile)
        {
            RpcConfiguration _configuration;
            EXPECT_THROW(
                TryGetRpcConfiguration(
                    "/nonexistent/manifest.arxml", "RpcServerEP", "ServerUnicastTcp",
                    _configuration),
                std::invalid_argument);
        }
    }
}
