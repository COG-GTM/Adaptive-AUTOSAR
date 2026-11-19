#include <gtest/gtest.h>
#include <utility>
#include <vector>
#include "../../../src/ara/phm/supervised_entity.h"
#include "./mocked_checkpoint_communicator.h"

namespace ara
{
    namespace phm
    {
        enum class DummyCheckpoint : uint32_t
        {
            None = 0,
            Startup = 1
        };

        TEST(SupervisedEntityTest, ReportCheckpointMehod)
        {
            const core::InstanceSpecifier cInstance("Instance0");
            const DummyCheckpoint cExpectedResult{DummyCheckpoint::Startup};

            MockedCheckpointCommunicator _communicator;
            DummyCheckpoint _actualResult{DummyCheckpoint::None};
            auto _callback =
                [&](uint32_t checkpointId) noexcept
            { _actualResult = static_cast<DummyCheckpoint>(checkpointId); };
            _communicator.SetCallback(_callback);

            SupervisedEntity _se(cInstance, &_communicator);
            _se.ReportCheckpoint(cExpectedResult);

            EXPECT_EQ(cExpectedResult, _actualResult);
        }

        TEST(SupervisedEntityTest, ReportMultipleCheckpoints)
        {
            const core::InstanceSpecifier cInstance("Instance0");
            const std::vector<uint32_t> cExpectedResults{1, 0, 1};

            MockedCheckpointCommunicator _communicator;
            std::vector<uint32_t> _actualResults;
            _communicator.SetCallback(
                [&_actualResults](uint32_t checkpointId)
                { _actualResults.push_back(checkpointId); });

            SupervisedEntity _se(cInstance, &_communicator);
            _se.ReportCheckpoint(DummyCheckpoint::Startup);
            _se.ReportCheckpoint(DummyCheckpoint::None);
            _se.ReportCheckpoint(DummyCheckpoint::Startup);

            EXPECT_EQ(cExpectedResults, _actualResults);
        }

        TEST(SupervisedEntityTest, ReportCheckpointWithoutCallback)
        {
            const core::InstanceSpecifier cInstance("Instance0");

            MockedCheckpointCommunicator _communicator;
            SupervisedEntity _se(cInstance, &_communicator);

            // The communicator fails to send, but the report should not throw.
            EXPECT_NO_THROW(_se.ReportCheckpoint(DummyCheckpoint::Startup));
        }

        TEST(SupervisedEntityTest, MoveConstructor)
        {
            const core::InstanceSpecifier cInstance("Instance0");
            const DummyCheckpoint cExpectedResult{DummyCheckpoint::Startup};

            MockedCheckpointCommunicator _communicator;
            DummyCheckpoint _actualResult{DummyCheckpoint::None};
            _communicator.SetCallback(
                [&_actualResult](uint32_t checkpointId)
                { _actualResult = static_cast<DummyCheckpoint>(checkpointId); });

            SupervisedEntity _se(cInstance, &_communicator);
            SupervisedEntity _movedSe{std::move(_se)};
            _movedSe.ReportCheckpoint(cExpectedResult);

            EXPECT_EQ(cExpectedResult, _actualResult);
        }

        TEST(SupervisedEntityTest, SharedCommunicatorScenario)
        {
            const core::InstanceSpecifier cFirstInstance("Instance0");
            const core::InstanceSpecifier cSecondInstance("Instance1");
            const uint32_t cExpectedResult{2};

            MockedCheckpointCommunicator _communicator;
            uint32_t _actualResult{0};
            _communicator.SetCallback(
                [&_actualResult](uint32_t checkpointId)
                { ++_actualResult; });

            SupervisedEntity _firstSe(cFirstInstance, &_communicator);
            SupervisedEntity _secondSe(cSecondInstance, &_communicator);
            _firstSe.ReportCheckpoint(DummyCheckpoint::Startup);
            _secondSe.ReportCheckpoint(DummyCheckpoint::Startup);

            EXPECT_EQ(cExpectedResult, _actualResult);
        }
    }
}
