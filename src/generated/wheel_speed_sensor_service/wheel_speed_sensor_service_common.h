// GENERATED FILE - DO NOT EDIT.
// Generator: autosar classic-to-adaptive bridge v0 (AE-29 prototype)
// Source:    Classic SWC /ComponentTypes/WheelSpeedSensor
// PROTOTYPE OUTPUT - not a certified toolchain, not for production or safety use.
//
// SOME/IP deployment identifiers and payload serialization shared by proxy and skeleton.

#ifndef WHEEL_SPEED_SENSOR_SERVICE_COMMON_H
#define WHEEL_SPEED_SENSOR_SERVICE_COMMON_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>
#include "../../ara/com/someip/someip_message.h"
#include "./wheel_speed_sensor_service_types.h"

namespace generated
{
    namespace wheelspeedsensorservice
    {
        /// @brief SOME/IP service interface ID
        const std::uint16_t cServiceId{0x1000};
        /// @brief Service interface major version (SOME/IP interface version)
        const std::uint8_t cMajorVersion{1};
        /// @brief Service interface minor version
        const std::uint32_t cMinorVersion{0};
        /// @brief SOME/IP protocol version
        const std::uint8_t cProtocolVersion{1};
        /// @brief Event group that contains all the events of the service
        const std::uint16_t cEventgroupId{0x0001};
        /// @brief Event ID of WheelSpeed (/PortInterfaces/WheelSpeed_I/WheelSpeed)
        const std::uint16_t cWheelSpeedEventId{0x8001};

        /// @brief Big-endian (network byte order) payload serialization of the basic types
        namespace serialization
        {
            /// @brief Append an unsigned integer in network byte order
            template <typename TUnsigned>
            void AppendBigEndian(std::vector<std::uint8_t> &payload, TUnsigned value)
            {
                for (std::size_t _index = sizeof(TUnsigned); _index > 0; --_index)
                {
                    payload.push_back(
                        static_cast<std::uint8_t>(value >> (8 * (_index - 1))));
                }
            }

            /// @brief Extract an unsigned integer in network byte order and advance the offset
            /// @returns False if the payload is too short, otherwise true
            template <typename TUnsigned>
            bool TryExtractBigEndian(
                const std::vector<std::uint8_t> &payload,
                std::size_t &offset,
                TUnsigned &value)
            {
                if (offset > payload.size() ||
                    payload.size() - offset < sizeof(TUnsigned))
                {
                    return false;
                }

                TUnsigned _result{0};
                for (std::size_t _index = 0; _index < sizeof(TUnsigned); ++_index)
                {
                    _result = static_cast<TUnsigned>(
                        (_result << 8) | payload[offset + _index]);
                }

                offset += sizeof(TUnsigned);
                value = _result;
                return true;
            }

            /// @brief Append a bool value to the payload
            inline void Serialize(
                std::vector<std::uint8_t> &payload, bool value)
            {
                payload.push_back(value ? 1 : 0);
            }

            /// @brief Extract a bool value from the payload and advance the offset
            /// @returns False if the payload is too short, otherwise true
            inline bool TryDeserialize(
                const std::vector<std::uint8_t> &payload,
                std::size_t &offset,
                bool &value)
            {
                std::uint8_t _raw;
                if (!TryExtractBigEndian(payload, offset, _raw))
                {
                    return false;
                }

                value = (_raw != 0);
                return true;
            }

            /// @brief Append a std::uint8_t value to the payload
            inline void Serialize(
                std::vector<std::uint8_t> &payload, std::uint8_t value)
            {
                AppendBigEndian(payload, value);
            }

            /// @brief Extract a std::uint8_t value from the payload and advance the offset
            /// @returns False if the payload is too short, otherwise true
            inline bool TryDeserialize(
                const std::vector<std::uint8_t> &payload,
                std::size_t &offset,
                std::uint8_t &value)
            {
                return TryExtractBigEndian(payload, offset, value);
            }

            /// @brief Append a std::uint16_t value to the payload
            inline void Serialize(
                std::vector<std::uint8_t> &payload, std::uint16_t value)
            {
                AppendBigEndian(payload, value);
            }

            /// @brief Extract a std::uint16_t value from the payload and advance the offset
            /// @returns False if the payload is too short, otherwise true
            inline bool TryDeserialize(
                const std::vector<std::uint8_t> &payload,
                std::size_t &offset,
                std::uint16_t &value)
            {
                return TryExtractBigEndian(payload, offset, value);
            }

            /// @brief Append a std::uint32_t value to the payload
            inline void Serialize(
                std::vector<std::uint8_t> &payload, std::uint32_t value)
            {
                AppendBigEndian(payload, value);
            }

            /// @brief Extract a std::uint32_t value from the payload and advance the offset
            /// @returns False if the payload is too short, otherwise true
            inline bool TryDeserialize(
                const std::vector<std::uint8_t> &payload,
                std::size_t &offset,
                std::uint32_t &value)
            {
                return TryExtractBigEndian(payload, offset, value);
            }

            /// @brief Append a std::uint64_t value to the payload
            inline void Serialize(
                std::vector<std::uint8_t> &payload, std::uint64_t value)
            {
                AppendBigEndian(payload, value);
            }

            /// @brief Extract a std::uint64_t value from the payload and advance the offset
            /// @returns False if the payload is too short, otherwise true
            inline bool TryDeserialize(
                const std::vector<std::uint8_t> &payload,
                std::size_t &offset,
                std::uint64_t &value)
            {
                return TryExtractBigEndian(payload, offset, value);
            }

            /// @brief Append a std::int8_t value to the payload
            inline void Serialize(
                std::vector<std::uint8_t> &payload, std::int8_t value)
            {
                AppendBigEndian(payload, static_cast<std::uint8_t>(value));
            }

            /// @brief Extract a std::int8_t value from the payload and advance the offset
            /// @returns False if the payload is too short, otherwise true
            inline bool TryDeserialize(
                const std::vector<std::uint8_t> &payload,
                std::size_t &offset,
                std::int8_t &value)
            {
                std::uint8_t _raw;
                if (!TryExtractBigEndian(payload, offset, _raw))
                {
                    return false;
                }

                value = static_cast<std::int8_t>(_raw);
                return true;
            }

            /// @brief Append a std::int16_t value to the payload
            inline void Serialize(
                std::vector<std::uint8_t> &payload, std::int16_t value)
            {
                AppendBigEndian(payload, static_cast<std::uint16_t>(value));
            }

            /// @brief Extract a std::int16_t value from the payload and advance the offset
            /// @returns False if the payload is too short, otherwise true
            inline bool TryDeserialize(
                const std::vector<std::uint8_t> &payload,
                std::size_t &offset,
                std::int16_t &value)
            {
                std::uint16_t _raw;
                if (!TryExtractBigEndian(payload, offset, _raw))
                {
                    return false;
                }

                value = static_cast<std::int16_t>(_raw);
                return true;
            }

            /// @brief Append a std::int32_t value to the payload
            inline void Serialize(
                std::vector<std::uint8_t> &payload, std::int32_t value)
            {
                AppendBigEndian(payload, static_cast<std::uint32_t>(value));
            }

            /// @brief Extract a std::int32_t value from the payload and advance the offset
            /// @returns False if the payload is too short, otherwise true
            inline bool TryDeserialize(
                const std::vector<std::uint8_t> &payload,
                std::size_t &offset,
                std::int32_t &value)
            {
                std::uint32_t _raw;
                if (!TryExtractBigEndian(payload, offset, _raw))
                {
                    return false;
                }

                value = static_cast<std::int32_t>(_raw);
                return true;
            }

            /// @brief Append a std::int64_t value to the payload
            inline void Serialize(
                std::vector<std::uint8_t> &payload, std::int64_t value)
            {
                AppendBigEndian(payload, static_cast<std::uint64_t>(value));
            }

            /// @brief Extract a std::int64_t value from the payload and advance the offset
            /// @returns False if the payload is too short, otherwise true
            inline bool TryDeserialize(
                const std::vector<std::uint8_t> &payload,
                std::size_t &offset,
                std::int64_t &value)
            {
                std::uint64_t _raw;
                if (!TryExtractBigEndian(payload, offset, _raw))
                {
                    return false;
                }

                value = static_cast<std::int64_t>(_raw);
                return true;
            }

            /// @brief Append a float value to the payload
            inline void Serialize(
                std::vector<std::uint8_t> &payload, float value)
            {
                std::uint32_t _raw;
                const std::uint8_t *cBytes{
                    reinterpret_cast<const std::uint8_t *>(&value)};
                std::copy(
                    cBytes, cBytes + sizeof(_raw),
                    reinterpret_cast<std::uint8_t *>(&_raw));
                AppendBigEndian(payload, _raw);
            }

            /// @brief Extract a float value from the payload and advance the offset
            /// @returns False if the payload is too short, otherwise true
            inline bool TryDeserialize(
                const std::vector<std::uint8_t> &payload,
                std::size_t &offset,
                float &value)
            {
                std::uint32_t _raw;
                if (!TryExtractBigEndian(payload, offset, _raw))
                {
                    return false;
                }

                const std::uint8_t *cBytes{
                    reinterpret_cast<const std::uint8_t *>(&_raw)};
                std::copy(
                    cBytes, cBytes + sizeof(value),
                    reinterpret_cast<std::uint8_t *>(&value));
                return true;
            }

            /// @brief Append a double value to the payload
            inline void Serialize(
                std::vector<std::uint8_t> &payload, double value)
            {
                std::uint64_t _raw;
                const std::uint8_t *cBytes{
                    reinterpret_cast<const std::uint8_t *>(&value)};
                std::copy(
                    cBytes, cBytes + sizeof(_raw),
                    reinterpret_cast<std::uint8_t *>(&_raw));
                AppendBigEndian(payload, _raw);
            }

            /// @brief Extract a double value from the payload and advance the offset
            /// @returns False if the payload is too short, otherwise true
            inline bool TryDeserialize(
                const std::vector<std::uint8_t> &payload,
                std::size_t &offset,
                double &value)
            {
                std::uint64_t _raw;
                if (!TryExtractBigEndian(payload, offset, _raw))
                {
                    return false;
                }

                const std::uint8_t *cBytes{
                    reinterpret_cast<const std::uint8_t *>(&_raw)};
                std::copy(
                    cBytes, cBytes + sizeof(value),
                    reinterpret_cast<std::uint8_t *>(&value));
                return true;
            }
        }

        /// @brief SOME/IP notification message that carries one event sample
        class EventMessage : public ara::com::someip::SomeIpMessage
        {
        private:
            static const std::size_t cHeaderSize{16};

            std::vector<std::uint8_t> mEventPayload;

        public:
            /// @brief Constructor
            /// @param eventId SOME/IP event ID
            /// @param sessionId Notification session ID
            /// @param eventPayload Serialized event sample
            EventMessage(
                std::uint16_t eventId,
                std::uint16_t sessionId,
                std::vector<std::uint8_t> eventPayload)
                : ara::com::someip::SomeIpMessage(
                      (static_cast<std::uint32_t>(cServiceId) << 16) | eventId,
                      0,
                      cProtocolVersion,
                      cMajorVersion,
                      ara::com::someip::SomeIpMessageType::Notification,
                      sessionId),
                  mEventPayload{std::move(eventPayload)}
            {
            }

            /// @brief SOME/IP length field: header remainder plus event payload
            std::uint32_t Length() const noexcept override
            {
                const std::uint32_t cHeaderRemainder{8};
                return cHeaderRemainder +
                       static_cast<std::uint32_t>(mEventPayload.size());
            }

            /// @brief Serialized message: SOME/IP header followed by the event payload
            std::vector<std::uint8_t> Payload() const override
            {
                std::vector<std::uint8_t> _result{
                    ara::com::someip::SomeIpMessage::Payload()};
                _result.insert(
                    _result.end(), mEventPayload.cbegin(), mEventPayload.cend());

                return _result;
            }

            /// @brief SOME/IP event ID (lower half of the message ID)
            std::uint16_t EventId() const noexcept
            {
                return static_cast<std::uint16_t>(MessageId() & 0xffff);
            }

            /// @brief SOME/IP service ID (upper half of the message ID)
            std::uint16_t ServiceId() const noexcept
            {
                return static_cast<std::uint16_t>(MessageId() >> 16);
            }

            /// @brief Serialized event sample
            const std::vector<std::uint8_t> &EventPayload() const noexcept
            {
                return mEventPayload;
            }

            /// @brief Deserialize a message from a byte vector
            /// @throws std::out_of_range Throws when the SOME/IP header is truncated
            ///         or the declared length exceeds the received bytes
            /// @note Bytes after the declared length (transport padding) are ignored.
            static EventMessage Deserialize(
                const std::vector<std::uint8_t> &payload)
            {
                if (payload.size() < cHeaderSize)
                {
                    throw std::out_of_range("Truncated SOME/IP header.");
                }

                // The length field covers the header remainder and the event payload
                const std::size_t cLengthOffset{4};
                const std::size_t cHeaderRemainder{8};
                std::size_t _offset{cLengthOffset};
                std::uint32_t _length{0};
                serialization::TryExtractBigEndian(payload, _offset, _length);
                if (_length < cHeaderRemainder || _length > payload.size() - _offset)
                {
                    throw std::out_of_range("Invalid SOME/IP length.");
                }

                EventMessage _result(0, 1, std::vector<std::uint8_t>());
                ara::com::someip::SomeIpMessage::Deserialize(&_result, payload);
                _result.mEventPayload.assign(
                    payload.cbegin() + cHeaderSize,
                    payload.cbegin() + _offset + _length);

                return _result;
            }
        };
    }
}

#endif
