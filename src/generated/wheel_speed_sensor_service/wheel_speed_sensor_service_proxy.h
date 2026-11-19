// GENERATED FILE - DO NOT EDIT.
// Generator: autosar classic-to-adaptive bridge v0 (AE-29 prototype)
// Source:    Classic SWC /ComponentTypes/WheelSpeedSensor
// PROTOTYPE OUTPUT - not a certified toolchain, not for production or safety use.
//
// Consumer side (proxy) of WheelSpeedSensorService.

#ifndef WHEEL_SPEED_SENSOR_SERVICE_PROXY_H
#define WHEEL_SPEED_SENSOR_SERVICE_PROXY_H

#include <cstdint>
#include <functional>
#include <vector>
#include "../../ara/com/helper/network_layer.h"
#include "./wheel_speed_sensor_service_common.h"

namespace generated
{
    namespace wheelspeedsensorservice
    {
        namespace proxy
        {
            /// @brief Consumer side of an event
            /// @tparam TSample Event sample type
            template <typename TSample>
            class Event
            {
            private:
                const std::uint16_t mEventId;
                bool mSubscribed;
                bool mHasSample;
                TSample mLastSample;
                std::function<void(const TSample &)> mReceiveHandler;

            public:
                /// @brief Event sample type
                using SampleType = TSample;

                /// @brief Constructor
                /// @param eventId SOME/IP event ID
                explicit Event(std::uint16_t eventId) : mEventId{eventId},
                                                        mSubscribed{false},
                                                        mHasSample{false},
                                                        mLastSample{}
                {
                }

                /// @brief Start accepting the event notifications
                /// @note Local filter only; no SOME/IP-SD eventgroup subscription
                void Subscribe() noexcept
                {
                    mSubscribed = true;
                }

                /// @brief Stop accepting the event notifications
                void Unsubscribe() noexcept
                {
                    mSubscribed = false;
                    mHasSample = false;
                }

                /// @brief Indicate whether the event notifications are accepted
                bool IsSubscribed() const noexcept
                {
                    return mSubscribed;
                }

                /// @brief Set a callback to be invoked on each received sample
                void SetReceiveHandler(std::function<void(const SampleType &)> handler)
                {
                    mReceiveHandler = std::move(handler);
                }

                /// @brief Remove the callback set by SetReceiveHandler
                void UnsetReceiveHandler()
                {
                    mReceiveHandler = nullptr;
                }

                /// @brief Try to get the last received sample
                /// @returns False if no sample is received since the subscription
                bool TryGetLastSample(SampleType &sample) const
                {
                    if (mHasSample)
                    {
                        sample = mLastSample;
                    }

                    return mHasSample;
                }

                /// @brief Process a notification received by the proxy
                /// @returns True if the notification is consumed by this event
                bool Process(const EventMessage &message)
                {
                    if (!mSubscribed ||
                        message.ServiceId() != cServiceId ||
                        message.EventId() != mEventId ||
                        message.ProtocolVersion() != cProtocolVersion ||
                        message.InterfaceVersion() != cMajorVersion ||
                        message.MessageType() !=
                            ara::com::someip::SomeIpMessageType::Notification)
                    {
                        return false;
                    }

                    std::size_t _offset{0};
                    SampleType _sample{};
                    if (!serialization::TryDeserialize(
                            message.EventPayload(), _offset, _sample))
                    {
                        return false;
                    }

                    mLastSample = _sample;
                    mHasSample = true;
                    if (mReceiveHandler)
                    {
                        mReceiveHandler(mLastSample);
                    }

                    return true;
                }
            };

            namespace events
            {
                /// @brief Classic data element /PortInterfaces/WheelSpeed_I/WheelSpeed
                using WheelSpeed = Event<WheelSpeed_T>;
            }

            /// @brief Proxy of WheelSpeedSensorService
            /// @note Generated from Classic SWC /ComponentTypes/WheelSpeedSensor
            /// @note No service discovery: the proxy is bound to the given transports.
            class WheelSpeedSensorServiceProxy
            {
            private:
                ara::com::helper::NetworkLayer<EventMessage> *const mEventNetworkLayer;

            public:
                /// @brief Event of Classic port /ComponentTypes/WheelSpeedSensor/WheelSpeed
                events::WheelSpeed WheelSpeed;

                /// @param eventNetworkLayer Transport of the event notifications
                explicit WheelSpeedSensorServiceProxy(
                    ara::com::helper::NetworkLayer<EventMessage> *eventNetworkLayer)
                    : mEventNetworkLayer{eventNetworkLayer},
                      WheelSpeed(cWheelSpeedEventId)
                {
                    mEventNetworkLayer->SetReceiver(
                        this,
                        [this](EventMessage message)
                        {
                            WheelSpeed.Process(message);
                        });
                }

                /// @brief Not copyable: transports keep a pointer to this object
                WheelSpeedSensorServiceProxy(const WheelSpeedSensorServiceProxy &) = delete;
                WheelSpeedSensorServiceProxy &operator=(const WheelSpeedSensorServiceProxy &) = delete;

                ~WheelSpeedSensorServiceProxy()
                {
                    // The network layer has no receiver removal; replace it with a no-op.
                    mEventNetworkLayer->SetReceiver(this, [](EventMessage) {});
                }
            };
        }
    }
}

#endif
