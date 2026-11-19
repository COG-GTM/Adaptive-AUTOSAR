// GENERATED FILE - DO NOT EDIT.
// Generator: autosar classic-to-adaptive bridge v0 (AE-29 prototype)
// Source:    Classic SWC /ComponentTypes/WheelSpeedSensor
// PROTOTYPE OUTPUT - not a certified toolchain, not for production or safety use.
//
// Provider side (skeleton) of WheelSpeedSensorService.

#ifndef WHEEL_SPEED_SENSOR_SERVICE_SKELETON_H
#define WHEEL_SPEED_SENSOR_SERVICE_SKELETON_H

#include <cstdint>
#include <vector>
#include "../../ara/com/helper/network_layer.h"
#include "./wheel_speed_sensor_service_common.h"

namespace generated
{
    namespace wheelspeedsensorservice
    {
        namespace skeleton
        {
            /// @brief Provider side of an event
            /// @tparam TSample Event sample type
            template <typename TSample>
            class Event
            {
            private:
                ara::com::helper::NetworkLayer<EventMessage> *const mNetworkLayer;
                const std::uint16_t mEventId;
                const bool *const mOffered;
                std::uint16_t mSessionId;

            public:
                using SampleType = TSample;

                Event(
                    ara::com::helper::NetworkLayer<EventMessage> *networkLayer,
                    std::uint16_t eventId,
                    const bool *offered) : mNetworkLayer{networkLayer},
                                           mEventId{eventId},
                                           mOffered{offered},
                                           mSessionId{1}
                {
                }

                /// @brief Send a sample as a SOME/IP notification
                /// @returns False if the service is not offered, otherwise true
                bool Send(const SampleType &sample)
                {
                    if (!*mOffered)
                    {
                        return false;
                    }

                    std::vector<std::uint8_t> _eventPayload;
                    serialization::Serialize(_eventPayload, sample);
                    EventMessage _message(mEventId, mSessionId, std::move(_eventPayload));
                    mNetworkLayer->Send(_message);
                    _message.IncrementSessionId();
                    mSessionId = _message.SessionId();

                    return true;
                }
            };

            namespace events
            {
                /// @brief Classic data element /PortInterfaces/WheelSpeed_I/WheelSpeed
                using WheelSpeed = Event<WheelSpeed_T>;
            }

            /// @brief Skeleton of WheelSpeedSensorService
            /// @note Generated from Classic SWC /ComponentTypes/WheelSpeedSensor
            /// @note OfferService only gates the events and methods of this object.
            ///       SOME/IP service discovery is not driven by the generated code.
            /// @note Not mapped (v0): /DataTypes/ImplementationDataTypes/WheelSpeed_T: CompuMethod /DataTypes/CompuMethods/WheelSpeed_T_CompuMethod (events and methods carry the raw internal value)
            class WheelSpeedSensorServiceSkeleton
            {
            private:
                bool mOffered;

            public:
                /// @brief Event of Classic port /ComponentTypes/WheelSpeedSensor/WheelSpeed
                events::WheelSpeed WheelSpeed;

                /// @param eventNetworkLayer Transport of the event notifications
                explicit WheelSpeedSensorServiceSkeleton(
                    ara::com::helper::NetworkLayer<EventMessage> *eventNetworkLayer)
                    : mOffered{false},
                      WheelSpeed(eventNetworkLayer, cWheelSpeedEventId, &mOffered)
                {
                }

                WheelSpeedSensorServiceSkeleton(const WheelSpeedSensorServiceSkeleton &) = delete;
                WheelSpeedSensorServiceSkeleton &operator=(const WheelSpeedSensorServiceSkeleton &) = delete;
                virtual ~WheelSpeedSensorServiceSkeleton() noexcept = default;

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
            };
        }
    }
}

#endif
