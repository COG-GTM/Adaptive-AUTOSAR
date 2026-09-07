#include <json/json.h>
#include <gtest/gtest.h>
#include "../../../src/ara/telemetry/telemetry_hub.h"

namespace ara
{
    namespace telemetry
    {
        class TelemetryHubTest : public testing::Test
        {
        protected:
            TelemetryHub mHub;

            Json::Value Snapshot(uint64_t sinceSequence = 0)
            {
                const std::string cSnapshot{mHub.SerializeSnapshot(sinceSequence)};

                Json::Value _result;
                Json::CharReaderBuilder _builder;
                std::istringstream _stream(cSnapshot);
                std::string _errors;

                EXPECT_TRUE(
                    Json::parseFromStream(_builder, _stream, &_result, &_errors))
                    << _errors;

                return _result;
            }

            void SetUp() override
            {
                mHub.Enable();
            }
        };

        TEST_F(TelemetryHubTest, DisabledCollection)
        {
            TelemetryHub _hub;
            _hub.PublishExecutionState("Application", "kRunning");

            EXPECT_FALSE(_hub.Enabled());
            EXPECT_EQ(0, _hub.Sequence());
        }

        TEST_F(TelemetryHubTest, ExecutionStatePublishing)
        {
            mHub.PublishExecutionState("ExecutionManagement", "kRunning");
            const Json::Value cSnapshot{Snapshot()};

            ASSERT_EQ(1, cSnapshot["applications"].size());
            EXPECT_EQ(
                "ExecutionManagement",
                cSnapshot["applications"][0]["name"].asString());
            EXPECT_EQ(
                "kRunning",
                cSnapshot["applications"][0]["executionState"].asString());
        }

        TEST_F(TelemetryHubTest, FunctionGroupTransitionRecording)
        {
            mHub.PublishFunctionGroupState("MachineFG", "Off");
            mHub.PublishFunctionGroupState("MachineFG", "Off");
            mHub.PublishFunctionGroupState("MachineFG", "StartUp");

            const Json::Value cSnapshot{Snapshot()};

            ASSERT_EQ(1, cSnapshot["functionGroups"].size());
            EXPECT_EQ(
                "StartUp", cSnapshot["functionGroups"][0]["state"].asString());

            // The repeated state should not be recorded as a transition.
            ASSERT_EQ(2, cSnapshot["transitions"].size());
            EXPECT_EQ("Off", cSnapshot["transitions"][0]["state"].asString());
            EXPECT_EQ("StartUp", cSnapshot["transitions"][1]["state"].asString());
        }

        TEST_F(TelemetryHubTest, CheckpointReporting)
        {
            mHub.RegisterCheckpoint(1, "AliveSupervision");
            mHub.PublishCheckpoint(1);
            mHub.PublishCheckpoint(1);

            const Json::Value cSnapshot{Snapshot()};

            ASSERT_EQ(1, cSnapshot["checkpoints"].size());
            EXPECT_EQ(1, cSnapshot["checkpoints"][0]["id"].asUInt());
            EXPECT_EQ(
                "AliveSupervision",
                cSnapshot["checkpoints"][0]["name"].asString());
            EXPECT_EQ(2, cSnapshot["checkpoints"][0]["reports"].asUInt64());
        }

        TEST_F(TelemetryHubTest, SupervisionStatusPublishing)
        {
            mHub.PublishSupervisionStatus("AliveSupervision", "kOk");
            mHub.PublishGlobalSupervisionStatus("kFailed", "AliveSupervision");

            const Json::Value cSnapshot{Snapshot()};

            ASSERT_EQ(1, cSnapshot["supervisions"].size());
            EXPECT_EQ("kOk", cSnapshot["supervisions"][0]["status"].asString());
            EXPECT_EQ(
                "kFailed", cSnapshot["globalSupervision"]["status"].asString());
            EXPECT_EQ(
                "AliveSupervision",
                cSnapshot["globalSupervision"]["dominantType"].asString());
        }

        TEST_F(TelemetryHubTest, UnchangedSupervisionStatusSequence)
        {
            mHub.PublishSupervisionStatus("AliveSupervision", "kOk");
            const uint64_t cSequence{mHub.Sequence()};
            mHub.PublishSupervisionStatus("AliveSupervision", "kOk");

            EXPECT_EQ(cSequence, mHub.Sequence());
        }

        TEST_F(TelemetryHubTest, SomeIpMessageRate)
        {
            mHub.PublishSomeIpMessage(SomeIpRecord{"tx", "client", 1, 2, 8});
            mHub.PublishSomeIpMessage(SomeIpRecord{"rx", "server", 1, 2, 4});

            const Json::Value cSnapshot{Snapshot()};

            EXPECT_EQ(2, cSnapshot["someip"]["total"].asUInt64());
            EXPECT_EQ(2, cSnapshot["someip"]["messages"].size());
            EXPECT_EQ(
                "tx", cSnapshot["someip"]["messages"][0]["direction"].asString());
            EXPECT_DOUBLE_EQ(
                0.4, cSnapshot["someip"]["ratePerSecond"].asDouble());
        }

        TEST_F(TelemetryHubTest, LogBuffering)
        {
            mHub.PublishLog(
                LogRecord{"ExecutionManagement", "main", "Info", "Started"});

            const Json::Value cSnapshot{Snapshot()};

            ASSERT_EQ(1, cSnapshot["logs"].size());
            EXPECT_EQ("Info", cSnapshot["logs"][0]["level"].asString());
            EXPECT_EQ("Started", cSnapshot["logs"][0]["message"].asString());
        }

        TEST_F(TelemetryHubTest, LogCapacity)
        {
            const std::size_t cCount{TelemetryHub::cLogCapacity + 10};

            for (std::size_t i = 0; i < cCount; ++i)
            {
                mHub.PublishLog(
                    LogRecord{"Application", "ctx", "Info", std::to_string(i)});
            }

            const Json::Value cSnapshot{Snapshot()};

            EXPECT_EQ(TelemetryHub::cLogCapacity, cSnapshot["logs"].size());
            EXPECT_EQ("10", cSnapshot["logs"][0]["message"].asString());
        }

        TEST_F(TelemetryHubTest, IncrementalSnapshot)
        {
            mHub.PublishLog(LogRecord{"Application", "ctx", "Info", "first"});
            const uint64_t cSequence{mHub.Sequence()};
            mHub.PublishLog(LogRecord{"Application", "ctx", "Info", "second"});

            const Json::Value cSnapshot{Snapshot(cSequence)};

            ASSERT_EQ(1, cSnapshot["logs"].size());
            EXPECT_EQ("second", cSnapshot["logs"][0]["message"].asString());
        }

        TEST_F(TelemetryHubTest, Resetting)
        {
            mHub.PublishExecutionState("Application", "kRunning");
            mHub.PublishLog(LogRecord{"Application", "ctx", "Info", "message"});
            mHub.Reset();

            const Json::Value cSnapshot{Snapshot()};

            EXPECT_EQ(0, mHub.Sequence());
            EXPECT_EQ(0, cSnapshot["applications"].size());
            EXPECT_EQ(0, cSnapshot["logs"].size());
            EXPECT_EQ(
                "kDeactivated",
                cSnapshot["globalSupervision"]["status"].asString());
        }
    }
}
