#include <json/json.h>
#include <gtest/gtest.h>
#include "../../../../src/ara/log/logger.h"
#include "../../../../src/ara/log/sink/telemetry_log_sink.h"

namespace ara
{
    namespace log
    {
        namespace sink
        {
            /// @brief Log sink which records the delegated log streams
            class RecordingLogSink : public LogSink
            {
            public:
                mutable std::vector<std::string> records;

                RecordingLogSink() : LogSink("Application", "Description")
                {
                }

                void Log(const LogStream &logStream) const override
                {
                    records.push_back(logStream.ToString());
                }
            };

            class TelemetryLogSinkTest : public testing::Test
            {
            protected:
                telemetry::TelemetryHub mHub;
                RecordingLogSink *mInnerSink;
                std::unique_ptr<TelemetryLogSink> mSink;

                LogStream GetLogStream(LogLevel level, std::string message) const
                {
                    const Logger cLogger{
                        Logger::CreateLogger("main", "Main context", LogLevel::kVerbose)};

                    LogStream _result{cLogger.WithLevel(level)};
                    _result << message;

                    return _result;
                }

                void SetUp() override
                {
                    std::unique_ptr<LogSink> _innerSink{new RecordingLogSink()};
                    mInnerSink = static_cast<RecordingLogSink *>(_innerSink.get());

                    mSink.reset(
                        new TelemetryLogSink(
                            std::move(_innerSink),
                            &mHub,
                            "ExecutionManagement",
                            "Execution management"));
                }
            };

            TEST_F(TelemetryLogSinkTest, InnerSinkDelegation)
            {
                mSink->Log(GetLogStream(LogLevel::kInfo, "Initialized"));

                ASSERT_EQ(1, mInnerSink->records.size());
                EXPECT_NE(
                    std::string::npos,
                    mInnerSink->records.at(0).find("Initialized"));
            }

            TEST_F(TelemetryLogSinkTest, DisabledHubPublishing)
            {
                mSink->Log(GetLogStream(LogLevel::kInfo, "Initialized"));

                EXPECT_EQ(1, mInnerSink->records.size());
                EXPECT_EQ(0, mHub.Sequence());
            }

            TEST_F(TelemetryLogSinkTest, EnabledHubPublishing)
            {
                mHub.Enable();
                mSink->Log(GetLogStream(LogLevel::kError, "Failed"));

                EXPECT_EQ(1, mHub.Sequence());
            }

            TEST_F(TelemetryLogSinkTest, StampedLogParsing)
            {
                const telemetry::LogRecord cRecord{
                    TelemetryLogSink::Parse(
                        "StateManagement",
                        GetLogStream(LogLevel::kWarn, "Degraded").ToString())};

                EXPECT_EQ("StateManagement", cRecord.application);
                EXPECT_EQ("main", cRecord.context);
                EXPECT_EQ("Warning", cRecord.level);
                EXPECT_EQ("Degraded", cRecord.message);
            }

            TEST_F(TelemetryLogSinkTest, UnstampedLogParsing)
            {
                const telemetry::LogRecord cRecord{
                    TelemetryLogSink::Parse("DiagnosticManager", "raw message")};

                EXPECT_EQ("DiagnosticManager", cRecord.application);
                EXPECT_TRUE(cRecord.context.empty());
                EXPECT_TRUE(cRecord.level.empty());
                EXPECT_EQ("raw message", cRecord.message);
            }
        }
    }
}
