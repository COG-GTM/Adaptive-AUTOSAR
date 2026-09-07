#ifndef TELEMETRY_LOG_SINK_H
#define TELEMETRY_LOG_SINK_H

#include <memory>
#include "../../telemetry/telemetry_hub.h"
#include "./log_sink.h"

namespace ara
{
    namespace log
    {
        namespace sink
        {
            /// @brief A log sink which publishes structured log records to a
            /// telemetry hub while delegating the logging to an inner sink
            /// @note The sink is NOT part of the Adaptive AUTOSAR standard.
            class TelemetryLogSink : public LogSink
            {
            private:
                std::unique_ptr<LogSink> mInnerSink;
                telemetry::TelemetryHub *const mHub;
                const std::string mApplicationId;

            public:
                /// @brief Constructor
                /// @param innerSink Sink which the logging is delegated to
                /// @param hub Telemetry hub which the structured records are published to
                /// @param appId Application ID
                /// @param appDescription Application description
                TelemetryLogSink(
                    std::unique_ptr<LogSink> innerSink,
                    telemetry::TelemetryHub *hub,
                    std::string appId,
                    std::string appDescription = "");

                TelemetryLogSink() = delete;

                /// @brief Delegate the log stream to the inner sink and publish
                /// its structured record to the telemetry hub
                /// @param logStream Stream to be logged
                void Log(const LogStream &logStream) const override;

                /// @brief Parse a logger stamped log stream into a structured record
                /// @param appId Application ID which emitted the log
                /// @param logString Serialized log stream content
                /// @returns Structured record of the given log stream
                static telemetry::LogRecord Parse(
                    std::string appId, const std::string &logString);
            };
        }
    }
}

#endif
