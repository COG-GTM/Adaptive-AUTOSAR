// GENERATED FILE - DO NOT EDIT.
// Generator: autosar classic-to-adaptive bridge v0 (AE-29 prototype)
// Source:    Classic SWC /ComponentTypes/TimerComponent
// PROTOTYPE OUTPUT - not a certified toolchain, not for production or safety use.
//
// SOME/IP deployment identifiers and payload serialization shared by proxy and skeleton.

#ifndef TIMER_COMPONENT_SERVICE_COMMON_H
#define TIMER_COMPONENT_SERVICE_COMMON_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>
#include "../../ara/com/someip/someip_message.h"
#include "./timer_component_service_types.h"

namespace generated
{
    namespace timercomponentservice
    {
        /// @brief SOME/IP service interface ID
        const std::uint16_t cServiceId{0x1001};
        /// @brief Service interface major version (SOME/IP interface version)
        const std::uint8_t cMajorVersion{1};
        /// @brief Service interface minor version
        const std::uint32_t cMinorVersion{0};
        /// @brief SOME/IP protocol version
        const std::uint8_t cProtocolVersion{1};
        /// @brief Method ID of GetTime (/PortInterfaces/FreeRunningTimer_I/GetTime)
        const std::uint16_t cGetTimeMethodId{0x0001};
        /// @brief Method ID of IsTimerElapsed (/PortInterfaces/FreeRunningTimer_I/IsTimerElapsed)
        const std::uint16_t cIsTimerElapsedMethodId{0x0002};

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
    }
}

#endif
