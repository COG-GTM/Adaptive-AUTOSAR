#ifndef TELEMETRY_HUB_H
#define TELEMETRY_HUB_H

#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <string>

namespace ara
{
    /// @brief Adaptive AUTOSAR runtime telemetry
    /// @note The namespace is NOT part of the Adaptive AUTOSAR standard.
    namespace telemetry
    {
        /// @brief Structured log record
        struct LogRecord
        {
            /// @brief Application ID which emitted the log
            std::string application;
            /// @brief Logging context ID
            std::string context;
            /// @brief Log severity level, e.g., "Info"
            std::string level;
            /// @brief Log message without any stamp
            std::string message;
        };

        /// @brief Structured SOME/IP message record
        struct SomeIpRecord
        {
            /// @brief Message direction, either "rx" or "tx"
            std::string direction;
            /// @brief Communication role, either "server" or "client"
            std::string role;
            /// @brief SOME/IP service ID
            uint16_t serviceId;
            /// @brief SOME/IP method ID
            uint16_t methodId;
            /// @brief RPC payload length in bytes
            uint32_t payloadLength;
        };

        /// @brief Process-wide store of the platform runtime telemetry
        /// @note Every mutating and reading member function is thread-safe.
        class TelemetryHub final
        {
        public:
            /// @brief Maximum number of the buffered log records
            static const std::size_t cLogCapacity{256};
            /// @brief Maximum number of the buffered SOME/IP message records
            static const std::size_t cSomeIpCapacity{64};
            /// @brief Maximum number of the buffered function group state transitions
            static const std::size_t cTransitionCapacity{32};
            /// @brief Time window in milliseconds to compute the SOME/IP message rate
            static const int64_t cRateWindowMs{5000};

            TelemetryHub();
            ~TelemetryHub() noexcept = default;

            TelemetryHub(const TelemetryHub &) = delete;
            TelemetryHub &operator=(const TelemetryHub &) = delete;

            /// @brief Get the process-wide hub instance
            /// @returns Reference to the singleton hub
            static TelemetryHub &Instance() noexcept;

            /// @brief Activate the telemetry collection
            /// @remark The collection is deactivated as long as no dashboard is served.
            void Enable() noexcept;

            /// @brief Indicate whether the telemetry collection is activated or not
            /// @returns True if the collection is activated; otherwise false
            bool Enabled() const noexcept;

            /// @brief Publish a structured log record
            /// @param record Log record to be buffered
            void PublishLog(LogRecord record);

            /// @brief Publish the execution state of a modelled process
            /// @param application Instance specifier of the adaptive application
            /// @param state Reported execution state, e.g., "kRunning"
            void PublishExecutionState(std::string application, std::string state);

            /// @brief Publish the current state of a function group
            /// @param functionGroup Function group short-name
            /// @param state Function group state short-name
            void PublishFunctionGroupState(
                std::string functionGroup, std::string state);

            /// @brief Register a supervision checkpoint from the health monitoring manifest
            /// @param id Checkpoint ID
            /// @param name Checkpoint short-name
            void RegisterCheckpoint(uint32_t id, std::string name);

            /// @brief Publish a supervision checkpoint report
            /// @param id Reported checkpoint ID
            void PublishCheckpoint(uint32_t id);

            /// @brief Publish the status of an elementary supervision
            /// @param supervision Supervision name, e.g., "AliveSupervision"
            /// @param status Supervision status, e.g., "kOk"
            void PublishSupervisionStatus(
                std::string supervision, std::string status);

            /// @brief Publish the global supervision status
            /// @param status Global supervision status, e.g., "kExpired"
            /// @param dominantType Supervision type which determined the status
            void PublishGlobalSupervisionStatus(
                std::string status, std::string dominantType);

            /// @brief Publish a SOME/IP message record
            /// @param record SOME/IP message record to be buffered
            void PublishSomeIpMessage(SomeIpRecord record);

            /// @brief Serialize the telemetry snapshot into a JSON document
            /// @param sinceSequence Sequence number of the last observed record
            /// @returns Serialized JSON document of the current snapshot
            /// @remark Records with a sequence number greater than the given one are included.
            std::string SerializeSnapshot(uint64_t sinceSequence) const;

            /// @brief Get the sequence number of the latest published record
            /// @returns Latest sequence number
            uint64_t Sequence() const;

            /// @brief Clear all the buffered records and the tracked states
            void Reset();

        private:
            struct StateEntry
            {
                std::string value;
                int64_t updatedAtMs;
            };

            struct CheckpointEntry
            {
                std::string name;
                uint64_t reports;
                int64_t lastReportMs;
            };

            struct SupervisionEntry
            {
                std::string status;
                std::string dominantType;
                int64_t updatedAtMs;
            };

            struct SequencedLog
            {
                uint64_t sequence;
                int64_t timestampMs;
                LogRecord record;
            };

            struct SequencedSomeIp
            {
                uint64_t sequence;
                int64_t timestampMs;
                SomeIpRecord record;
            };

            struct SequencedTransition
            {
                uint64_t sequence;
                int64_t timestampMs;
                std::string functionGroup;
                std::string state;
            };

            static int64_t nowMs() noexcept;

            uint64_t nextSequence();
            double someIpRate(int64_t nowMs) const;

            mutable std::mutex mMutex;
            bool mEnabled;
            uint64_t mSequence;
            int64_t mStartedAtMs;
            uint64_t mSomeIpTotal;
            std::map<std::string, StateEntry> mExecutionStates;
            std::map<std::string, StateEntry> mFunctionGroupStates;
            std::map<uint32_t, CheckpointEntry> mCheckpoints;
            std::map<std::string, SupervisionEntry> mSupervisions;
            SupervisionEntry mGlobalSupervision;
            std::deque<SequencedLog> mLogs;
            std::deque<SequencedSomeIp> mSomeIpMessages;
            std::deque<SequencedTransition> mTransitions;
            std::deque<int64_t> mSomeIpTimestamps;
        };
    }
}

#endif
