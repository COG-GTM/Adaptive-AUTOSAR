#include <gtest/gtest.h>
#include <vector>
#include "./mocked_checkpoint_communicator.h"

namespace ara
{
    namespace phm
    {
        TEST(CheckpointCommunicatorTest, TrySendWithoutCallback)
        {
            const uint32_t cCheckpoint{1};

            MockedCheckpointCommunicator _communicator;
            const bool cSent{_communicator.TrySend(cCheckpoint)};

            EXPECT_FALSE(cSent);
        }

        TEST(CheckpointCommunicatorTest, SetCallbackMethod)
        {
            const std::vector<uint32_t> cExpectedResults{1, 0, 0xffffffff};

            MockedCheckpointCommunicator _communicator;
            std::vector<uint32_t> _actualResults;
            _communicator.SetCallback(
                [&_actualResults](uint32_t checkpoint)
                { _actualResults.push_back(checkpoint); });

            for (uint32_t expectedResult : cExpectedResults)
            {
                const bool cSent{_communicator.TrySend(expectedResult)};
                EXPECT_TRUE(cSent);
            }

            EXPECT_EQ(cExpectedResults, _actualResults);
        }

        TEST(CheckpointCommunicatorTest, SetCallbackReplacesPreviousCallback)
        {
            const uint32_t cCheckpoint{7};

            MockedCheckpointCommunicator _communicator;
            uint32_t _firstCounter{0};
            uint32_t _secondCounter{0};
            _communicator.SetCallback(
                [&_firstCounter](uint32_t checkpoint)
                { ++_firstCounter; });
            _communicator.SetCallback(
                [&_secondCounter](uint32_t checkpoint)
                { ++_secondCounter; });

            _communicator.TrySend(cCheckpoint);

            EXPECT_EQ(0U, _firstCounter);
            EXPECT_EQ(1U, _secondCounter);
        }

        TEST(CheckpointCommunicatorTest, ResetCallbackMethod)
        {
            const uint32_t cCheckpoint{1};

            MockedCheckpointCommunicator _communicator;
            uint32_t _counter{0};
            _communicator.SetCallback(
                [&_counter](uint32_t checkpoint)
                { ++_counter; });

            EXPECT_TRUE(_communicator.TrySend(cCheckpoint));
            _communicator.ResetCallback();
            EXPECT_FALSE(_communicator.TrySend(cCheckpoint));
            EXPECT_EQ(1U, _counter);

            // Resetting an already reset callback should be safe
            _communicator.ResetCallback();
            EXPECT_FALSE(_communicator.TrySend(cCheckpoint));
        }
    }
}
