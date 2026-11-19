// GENERATED FILE - DO NOT EDIT.
// Generator: autosar classic-to-adaptive bridge v0 (AE-29 prototype)
// Source:    Classic SWC /ComponentTypes/TimerComponent
// PROTOTYPE OUTPUT - not a certified toolchain, not for production or safety use.
//
// Consumer side (proxy) of TimerComponentService.

#ifndef TIMER_COMPONENT_SERVICE_PROXY_H
#define TIMER_COMPONENT_SERVICE_PROXY_H

#include <cstdint>
#include <functional>
#include <vector>
#include "../../ara/com/someip/rpc/rpc_client.h"
#include "./timer_component_service_common.h"

namespace generated
{
    namespace timercomponentservice
    {
        namespace proxy
        {
            /// @brief Proxy of TimerComponentService
            /// @note Generated from Classic SWC /ComponentTypes/TimerComponent
            /// @note No service discovery: the proxy is bound to the given transports.
            /// @note Methods report their result via a callback (no ara::core::Future).
            class TimerComponentServiceProxy
            {
            private:
                ara::com::someip::rpc::RpcClient *const mRpcClient;
                const std::uint16_t mClientId;

            public:
                /// @brief Response callback of GetTime: success flag, then the OUT arguments
                using GetTimeHandler = std::function<void(bool, uint32)>;
                /// @brief Response callback of IsTimerElapsed: success flag, then the OUT arguments
                using IsTimerElapsedHandler = std::function<void(bool, boolean)>;

                /// @param rpcClient RPC client that sends the method requests
                /// @param clientId SOME/IP client ID of this proxy
                TimerComponentServiceProxy(
                    ara::com::someip::rpc::RpcClient *rpcClient,
                    std::uint16_t clientId)
                    : mRpcClient{rpcClient},
                      mClientId{clientId}
                {
                }

                /// @brief Not copyable: transports keep a pointer to this object
                TimerComponentServiceProxy(const TimerComponentServiceProxy &) = delete;
                TimerComponentServiceProxy &operator=(const TimerComponentServiceProxy &) = delete;

                ~TimerComponentServiceProxy() noexcept = default;

                /// @brief Classic operation /PortInterfaces/FreeRunningTimer_I/GetTime
                void GetTime(GetTimeHandler responseHandler)
                {
                    std::vector<std::uint8_t> _request;

                    mRpcClient->SetHandler(
                        cServiceId, cGetTimeMethodId,
                        [responseHandler](
                            const ara::com::someip::rpc::SomeIpRpcMessage &message)
                        {
                            bool _successful{
                                message.MessageType() ==
                                    ara::com::someip::SomeIpMessageType::Response &&
                                message.ReturnCode() ==
                                    ara::com::someip::SomeIpReturnCode::eOK};
                            std::size_t _offset{0};
                            uint32 _value{};
                            _successful =
                                _successful &&
                                serialization::TryDeserialize(
                                    message.RpcPayload(), _offset, _value);
                            responseHandler(_successful, _value);
                        });
                    mRpcClient->Send(
                        cServiceId, cGetTimeMethodId, mClientId, _request);
                }

                /// @brief Classic operation /PortInterfaces/FreeRunningTimer_I/IsTimerElapsed
                void IsTimerElapsed(uint32 startTime, uint32 duration, IsTimerElapsedHandler responseHandler)
                {
                    std::vector<std::uint8_t> _request;
                    serialization::Serialize(_request, startTime);
                    serialization::Serialize(_request, duration);

                    mRpcClient->SetHandler(
                        cServiceId, cIsTimerElapsedMethodId,
                        [responseHandler](
                            const ara::com::someip::rpc::SomeIpRpcMessage &message)
                        {
                            bool _successful{
                                message.MessageType() ==
                                    ara::com::someip::SomeIpMessageType::Response &&
                                message.ReturnCode() ==
                                    ara::com::someip::SomeIpReturnCode::eOK};
                            std::size_t _offset{0};
                            boolean _result{};
                            _successful =
                                _successful &&
                                serialization::TryDeserialize(
                                    message.RpcPayload(), _offset, _result);
                            responseHandler(_successful, _result);
                        });
                    mRpcClient->Send(
                        cServiceId, cIsTimerElapsedMethodId, mClientId, _request);
                }
            };
        }
    }
}

#endif
