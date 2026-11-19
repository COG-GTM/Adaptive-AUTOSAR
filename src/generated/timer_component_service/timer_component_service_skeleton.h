// GENERATED FILE - DO NOT EDIT.
// Generator: autosar classic-to-adaptive bridge v0 (AE-29 prototype)
// Source:    Classic SWC /ComponentTypes/TimerComponent
// PROTOTYPE OUTPUT - not a certified toolchain, not for production or safety use.
//
// Provider side (skeleton) of TimerComponentService.

#ifndef TIMER_COMPONENT_SERVICE_SKELETON_H
#define TIMER_COMPONENT_SERVICE_SKELETON_H

#include <cstdint>
#include <vector>
#include "../../ara/com/someip/rpc/rpc_server.h"
#include "./timer_component_service_common.h"

namespace generated
{
    namespace timercomponentservice
    {
        namespace skeleton
        {
            /// @brief Skeleton of TimerComponentService
            /// @note Generated from Classic SWC /ComponentTypes/TimerComponent
            /// @note OfferService only gates the events and methods of this object.
            ///       SOME/IP service discovery is not driven by the generated code.
            /// @note Not mapped (v0): /AUTOSAR_Platform/ImplementationDataTypes/boolean: CompuMethod /AUTOSAR_Platform/CompuMethods/boolean_CompuMethod (events and methods carry the raw internal value)
            class TimerComponentServiceSkeleton
            {
            private:
                bool mOffered;
                ara::com::someip::rpc::RpcServer *const mRpcServer;

                bool handleGetTime(
                    const std::vector<std::uint8_t> &request,
                    std::vector<std::uint8_t> &response)
                {
                    if (!mOffered)
                    {
                        return false;
                    }

                    static_cast<void>(request);
                    uint32 _value{};

                    const bool cSuccessful{GetTime(_value)};
                    if (cSuccessful)
                    {
                        serialization::Serialize(response, _value);
                    }

                    return cSuccessful;
                }

                bool handleIsTimerElapsed(
                    const std::vector<std::uint8_t> &request,
                    std::vector<std::uint8_t> &response)
                {
                    if (!mOffered)
                    {
                        return false;
                    }

                    std::size_t _offset{0};
                    uint32 _startTime{};
                    uint32 _duration{};
                    boolean _result{};
                    if (!serialization::TryDeserialize(request, _offset, _startTime))
                    {
                        return false;
                    }
                    if (!serialization::TryDeserialize(request, _offset, _duration))
                    {
                        return false;
                    }

                    const bool cSuccessful{IsTimerElapsed(_startTime, _duration, _result)};
                    if (cSuccessful)
                    {
                        serialization::Serialize(response, _result);
                    }

                    return cSuccessful;
                }

            public:
                /// @param rpcServer RPC server that dispatches the method requests
                explicit TimerComponentServiceSkeleton(
                    ara::com::someip::rpc::RpcServer *rpcServer)
                    : mOffered{false},
                      mRpcServer{rpcServer}
                {
                    mRpcServer->SetHandler(
                        cServiceId, cGetTimeMethodId,
                        [this](const std::vector<std::uint8_t> &request,
                               std::vector<std::uint8_t> &response)
                        {
                            return handleGetTime(request, response);
                        });
                    mRpcServer->SetHandler(
                        cServiceId, cIsTimerElapsedMethodId,
                        [this](const std::vector<std::uint8_t> &request,
                               std::vector<std::uint8_t> &response)
                        {
                            return handleIsTimerElapsed(request, response);
                        });
                }

                /// @brief Not copyable: transports keep a pointer to this object
                TimerComponentServiceSkeleton(const TimerComponentServiceSkeleton &) = delete;
                TimerComponentServiceSkeleton &operator=(const TimerComponentServiceSkeleton &) = delete;

                virtual ~TimerComponentServiceSkeleton()
                {
                    // The RPC server has no handler removal; replace them with rejections.
                    mRpcServer->SetHandler(
                        cServiceId, cGetTimeMethodId,
                        [](const std::vector<std::uint8_t> &,
                           std::vector<std::uint8_t> &)
                        {
                            return false;
                        });
                    mRpcServer->SetHandler(
                        cServiceId, cIsTimerElapsedMethodId,
                        [](const std::vector<std::uint8_t> &,
                           std::vector<std::uint8_t> &)
                        {
                            return false;
                        });
                }

                /// @brief Enable the events and the methods of the service
                void OfferService() noexcept
                {
                    mOffered = true;
                }

                /// @brief Disable the events and the methods of the service
                void StopOfferService() noexcept
                {
                    mOffered = false;
                }

                /// @brief Indicate whether the service is offered
                bool IsOffered() const noexcept
                {
                    return mOffered;
                }

                /// @brief Classic operation /PortInterfaces/FreeRunningTimer_I/GetTime
                /// @returns True if the call is handled; false is reported as E_NOT_OK
                virtual bool GetTime(uint32 &value) = 0;

                /// @brief Classic operation /PortInterfaces/FreeRunningTimer_I/IsTimerElapsed
                /// @returns True if the call is handled; false is reported as E_NOT_OK
                virtual bool IsTimerElapsed(uint32 startTime, uint32 duration, boolean &result) = 0;
            };
        }
    }
}

#endif
