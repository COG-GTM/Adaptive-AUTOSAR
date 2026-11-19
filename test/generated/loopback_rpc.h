#ifndef LOOPBACK_RPC_H
#define LOOPBACK_RPC_H

#include "../../src/ara/com/someip/rpc/rpc_client.h"
#include "../../src/ara/com/someip/rpc/rpc_server.h"

namespace generated
{
    /// @brief In-process RPC server that exposes the request dispatching
    /// @note Hand-written test helper (not generated)
    class LoopbackRpcServer : public ara::com::someip::rpc::RpcServer
    {
    public:
        LoopbackRpcServer(
            uint8_t protocolVersion,
            uint8_t interfaceVersion) noexcept : ara::com::someip::rpc::RpcServer(protocolVersion, interfaceVersion)
        {
        }

        using ara::com::someip::rpc::RpcServer::TryInvokeHandler;
    };

    /// @brief In-process RPC client that forwards the serialized requests to a loopback server
    /// @note Hand-written test helper (not generated)
    class LoopbackRpcClient : public ara::com::someip::rpc::RpcClient
    {
    private:
        const LoopbackRpcServer *const mServer;

    protected:
        void Send(const std::vector<uint8_t> &payload) override
        {
            std::vector<uint8_t> _responsePayload;
            bool _handled{mServer->TryInvokeHandler(payload, _responsePayload)};
            if (_handled)
            {
                InvokeHandler(_responsePayload);
            }
        }

    public:
        LoopbackRpcClient(
            const LoopbackRpcServer *server,
            uint8_t protocolVersion,
            uint8_t interfaceVersion) noexcept : ara::com::someip::rpc::RpcClient(protocolVersion, interfaceVersion),
                                                 mServer{server}
        {
        }
    };
}

#endif
