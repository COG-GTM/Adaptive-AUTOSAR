#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include "../../../src/ara/log/sink/file_log_sink.h"

namespace ara
{
    namespace log
    {
        namespace sink
        {
            class FileLogSinkTest : public testing::Test
            {
            protected:
                const std::string cAppId{"APP01"};
                const std::string cAppDescription{"Test application"};
                std::string LogFilePath;

                void SetUp() override
                {
                    char _pathTemplate[]{"/tmp/file_log_sink_test_XXXXXX"};
                    const int cFileDescriptor{mkstemp(_pathTemplate)};
                    ASSERT_NE(-1, cFileDescriptor);
                    close(cFileDescriptor);
                    LogFilePath = _pathTemplate;
                }

                void TearDown() override
                {
                    std::remove(LogFilePath.c_str());
                }

                std::string ReadLogFile() const
                {
                    std::ifstream _logFile(LogFilePath);
                    std::stringstream _buffer;
                    _buffer << _logFile.rdbuf();

                    return _buffer.str();
                }
            };

            TEST_F(FileLogSinkTest, LogMethod)
            {
                const std::string cLogMessage{"Hello file sink"};

                FileLogSink _sink(cAppId, cAppDescription, LogFilePath);
                LogStream _logStream;
                _logStream << cLogMessage;
                _sink.Log(_logStream);

                const std::string cContent{ReadLogFile()};
                const std::string cExpectedSuffix{
                    cAppId + " " + cAppDescription + " " + cLogMessage + "\n"};

                ASSERT_GE(cContent.size(), cExpectedSuffix.size());
                EXPECT_EQ(
                    cExpectedSuffix,
                    cContent.substr(cContent.size() - cExpectedSuffix.size()));
            }

            TEST_F(FileLogSinkTest, EmptyAppDescription)
            {
                const std::string cLogMessage{"No description"};

                FileLogSink _sink(cAppId, "", LogFilePath);
                LogStream _logStream;
                _logStream << cLogMessage;
                _sink.Log(_logStream);

                const std::string cContent{ReadLogFile()};
                const std::string cExpectedSuffix{cAppId + " " + cLogMessage + "\n"};

                ASSERT_GE(cContent.size(), cExpectedSuffix.size());
                EXPECT_EQ(
                    cExpectedSuffix,
                    cContent.substr(cContent.size() - cExpectedSuffix.size()));
            }

            TEST_F(FileLogSinkTest, AppendMode)
            {
                const std::string cFirstMessage{"First entry"};
                const std::string cSecondMessage{"Second entry"};

                FileLogSink _sink(cAppId, cAppDescription, LogFilePath);

                LogStream _firstLogStream;
                _firstLogStream << cFirstMessage;
                _sink.Log(_firstLogStream);

                LogStream _secondLogStream;
                _secondLogStream << cSecondMessage;
                _sink.Log(_secondLogStream);

                const std::string cContent{ReadLogFile()};
                const size_t cFirstPosition{cContent.find(cFirstMessage)};
                const size_t cSecondPosition{cContent.find(cSecondMessage)};

                ASSERT_NE(std::string::npos, cFirstPosition);
                ASSERT_NE(std::string::npos, cSecondPosition);
                EXPECT_LT(cFirstPosition, cSecondPosition);
            }

            TEST_F(FileLogSinkTest, PreservesExistingContent)
            {
                const std::string cExistingContent{"existing line\n"};
                const std::string cLogMessage{"Appended entry"};

                {
                    std::ofstream _logFile(LogFilePath);
                    _logFile << cExistingContent;
                }

                FileLogSink _sink(cAppId, cAppDescription, LogFilePath);
                LogStream _logStream;
                _logStream << cLogMessage;
                _sink.Log(_logStream);

                const std::string cContent{ReadLogFile()};
                EXPECT_EQ(0, cContent.find(cExistingContent));
                EXPECT_NE(std::string::npos, cContent.find(cLogMessage));
            }
        }
    }
}
