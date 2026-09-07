#include "./telemetry_log_sink.h"

namespace ara
{
    namespace log
    {
        namespace sink
        {
            namespace
            {
                const std::string cContextIdPrefix{"Context ID:"};
                const std::string cContextDescriptionPrefix{"Context Description:"};
                const std::string cLogLevelPrefix{"Log Level:"};
                const char cSeparator{';'};

                bool tryTakeField(
                    const std::string &logString,
                    const std::string &prefix,
                    std::size_t &position,
                    std::string &field)
                {
                    if (logString.compare(position, prefix.size(), prefix) != 0)
                    {
                        return false;
                    }

                    const std::size_t cValueStart{position + prefix.size()};
                    const std::size_t cValueEnd{
                        logString.find(cSeparator, cValueStart)};

                    if (cValueEnd == std::string::npos)
                    {
                        return false;
                    }

                    field = logString.substr(cValueStart, cValueEnd - cValueStart);
                    position = cValueEnd + 1;

                    return true;
                }
            }

            TelemetryLogSink::TelemetryLogSink(
                std::unique_ptr<LogSink> innerSink,
                telemetry::TelemetryHub *hub,
                std::string appId,
                std::string appDescription) : LogSink(appId, appDescription),
                                              mInnerSink{std::move(innerSink)},
                                              mHub{hub},
                                              mApplicationId{appId}
            {
            }

            void TelemetryLogSink::Log(const LogStream &logStream) const
            {
                mInnerSink->Log(logStream);

                if (mHub->Enabled())
                {
                    mHub->PublishLog(Parse(mApplicationId, logStream.ToString()));
                }
            }

            telemetry::LogRecord TelemetryLogSink::Parse(
                std::string appId, const std::string &logString)
            {
                telemetry::LogRecord _result;
                _result.application = std::move(appId);

                std::size_t _position{0};
                std::string _contextDescription;

                const bool cParsed{
                    tryTakeField(
                        logString, cContextIdPrefix, _position, _result.context) &&
                    tryTakeField(
                        logString,
                        cContextDescriptionPrefix,
                        _position,
                        _contextDescription) &&
                    tryTakeField(
                        logString, cLogLevelPrefix, _position, _result.level)};

                if (cParsed)
                {
                    _result.message = logString.substr(_position);
                }
                else
                {
                    _result.message = logString;
                }

                return _result;
            }
        }
    }
}
