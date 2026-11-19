/// @file Loopback integration harness: runs the real DoipServer
/// (DoipServer -> DiagMessageHandler -> ara::diag::routing -> ReadDataByIdentifier)
/// on TCP 127.0.0.1:8081 and exchanges raw DoIP diagnostic messages with it.
/// @note This is loopback integration coverage, not an end-to-end vehicle test.
///       No vehicle cloud API and no serial I/O is involved; the VIN is a stand-in.

#include <csignal>
#include <cstdio>
#include <cstring>
#include <string>
#include "./loopback_doip_server.h"

namespace
{
    volatile std::sig_atomic_t gTerminated{0};

    void onSignal(int)
    {
        gTerminated = 1;
    }

    void printBytes(const char *prefix, const std::vector<uint8_t> &bytes)
    {
        std::printf("%s", prefix);
        for (uint8_t byte : bytes)
        {
            std::printf(" %02X", byte);
        }
        std::printf("\n");
    }

    bool tryRunCase(
        application::doip::LoopbackDoipClient &client,
        const char *title,
        const std::vector<uint8_t> &udsRequest,
        const std::vector<uint8_t> &expectedUdsResponse)
    {
        // DoIP header (diagnostic message 0x8001) + SA 0x0E80 + TA 0x0001
        const size_t cAddressingSize{4};
        // DoIP header + SA + TA + ACK code precede the UDS response within the ACK (0x8002)
        const size_t cUdsResponseOffset{13};

        const size_t cPayloadLength{cAddressingSize + udsRequest.size()};
        std::vector<uint8_t> _request{
            0x02, 0xfd, 0x80, 0x01,
            0x00, 0x00, 0x00, static_cast<uint8_t>(cPayloadLength),
            0x0e, 0x80, 0x00, 0x01};
        _request.insert(_request.end(), udsRequest.cbegin(), udsRequest.cend());

        std::printf("[%s]\n", title);
        printBytes("  TX DoIP:", _request);

        std::vector<uint8_t> _response;
        const bool cExchanged{
            client.TryExchange(_request, _response)};

        if (!cExchanged || _response.size() < cUdsResponseOffset)
        {
            std::printf("  RX DoIP: <no complete response>\n  FAIL\n");
            return false;
        }

        printBytes("  RX DoIP:", _response);

        const std::vector<uint8_t> cUdsResponse(
            _response.cbegin() + cUdsResponseOffset, _response.cend());
        printBytes("  RX UDS: ", cUdsResponse);

        const bool cResult{cUdsResponse == expectedUdsResponse};
        std::printf("  %s\n", cResult ? "PASS" : "FAIL");

        return cResult;
    }
}

int main(int argc, char *argv[])
{
    const uint16_t cPort{8081};
    const std::string cVin{"YV1LOOPBACKTEST01"};
    const bool cServeOnly{argc > 1 && std::strcmp(argv[1], "--serve") == 0};

    application::doip::LoopbackDoipServer _server(cPort, cVin);
    std::printf(
        "DoipServer is listening on %s:%u (stand-in VIN: %s)\n",
        application::doip::LoopbackDoipServer::cIpAddress,
        static_cast<unsigned int>(cPort),
        cVin.c_str());
    std::fflush(stdout);

    if (cServeOnly)
    {
        // Keep serving external testers until being interrupted
        std::signal(SIGINT, onSignal);
        std::signal(SIGTERM, onSignal);

        while (!gTerminated)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        return 0;
    }

    application::doip::LoopbackDoipClient _client(cPort);

    std::vector<uint8_t> _vinResponse{0x62, 0xf1, 0x90};
    _vinResponse.insert(_vinResponse.end(), cVin.cbegin(), cVin.cend());

    bool _result{
        tryRunCase(
            _client,
            "Positive response: read DID 0xF190 (VIN)",
            {0x22, 0xf1, 0x90},
            _vinResponse)};

    _result &=
        tryRunCase(
            _client,
            "NRC 0x31 (request out of range): read unknown DID 0xF191",
            {0x22, 0xf1, 0x91},
            {0x7f, 0x22, 0x31});

    _result &=
        tryRunCase(
            _client,
            "NRC 0x22 (conditions not correct): read DID 0xF50D while its resource is unreachable",
            {0x22, 0xf5, 0x0d},
            {0x7f, 0x22, 0x22});

    return _result ? 0 : 1;
}
