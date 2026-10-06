#include <gtest/gtest.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mutex>
#include <vector>
#include "../../../src/application/helper/fifo_checkpoint_communicator.h"
#include "./testable_fixture.h"

namespace application
{
    namespace helper
    {
        class FifoCheckpointCommunicatorTest : public testing::Test
        {
        protected:
            fixture::TemporaryDirectory Directory;
            AsyncBsdSocketLib::Poller Poller;
            fixture::PollingThread PollingThread;
            std::mutex Mutex;
            std::vector<uint32_t> ReceivedCheckpoints;

            FifoCheckpointCommunicatorTest() : PollingThread(&Poller)
            {
            }

            std::string GetFifoPath() const
            {
                return Directory.GetFilePath("fifo_communicator");
            }

            void OnCheckpoint(uint32_t checkpoint)
            {
                std::lock_guard<std::mutex> _lock(Mutex);
                ReceivedCheckpoints.push_back(checkpoint);
            }

            size_t GetReceivedCount()
            {
                std::lock_guard<std::mutex> _lock(Mutex);
                return ReceivedCheckpoints.size();
            }
        };

        TEST_F(FifoCheckpointCommunicatorTest, CreateFifo)
        {
            FifoCheckpointCommunicator _communicator(&Poller, GetFifoPath());

            struct stat _status;
            ASSERT_EQ(0, stat(GetFifoPath().c_str(), &_status));
            EXPECT_TRUE(S_ISFIFO(_status.st_mode));
        }

        TEST_F(FifoCheckpointCommunicatorTest, InvalidFifoPath)
        {
            EXPECT_THROW(
                FifoCheckpointCommunicator _communicator(
                    &Poller, Directory.GetFilePath("missing/fifo")),
                std::runtime_error);
        }

        TEST_F(FifoCheckpointCommunicatorTest, SendAndReceiveCheckpoints)
        {
            FifoCheckpointCommunicator _communicator(&Poller, GetFifoPath());
            _communicator.SetCallback(
                [this](uint32_t checkpoint)
                { OnCheckpoint(checkpoint); });

            PollingThread.Start();

            // The edge-triggered receiver reads one frame per FIFO write,
            // so each checkpoint is awaited before sending the next one.
            const std::vector<uint32_t> cCheckpoints{1, 2, 0xdeadbeef};
            for (size_t i = 0; i < cCheckpoints.size(); ++i)
            {
                EXPECT_TRUE(_communicator.TrySend(cCheckpoints[i]));
                EXPECT_TRUE(
                    fixture::WaitUntil(
                        [this, i]()
                        { return GetReceivedCount() > i; }));
            }

            PollingThread.Stop();

            std::lock_guard<std::mutex> _lock(Mutex);
            EXPECT_EQ(cCheckpoints, ReceivedCheckpoints);
        }

        TEST_F(FifoCheckpointCommunicatorTest, ResetCallback)
        {
            FifoCheckpointCommunicator _communicator(&Poller, GetFifoPath());
            _communicator.SetCallback(
                [this](uint32_t checkpoint)
                { OnCheckpoint(checkpoint); });
            _communicator.ResetCallback();

            EXPECT_TRUE(_communicator.TrySend(1));
            PollingThread.Start();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            PollingThread.Stop();

            EXPECT_EQ(0, GetReceivedCount());
        }

        TEST_F(FifoCheckpointCommunicatorTest, CorruptedPayloadIsDropped)
        {
            FifoCheckpointCommunicator _communicator(&Poller, GetFifoPath());
            _communicator.SetCallback(
                [this](uint32_t checkpoint)
                { OnCheckpoint(checkpoint); });

            // Write a payload with an invalid E2E CRC directly to the FIFO
            const int cWriter{open(GetFifoPath().c_str(), O_WRONLY | O_NONBLOCK)};
            ASSERT_GE(cWriter, 0);
            const uint8_t cCorrupted[]{0x00, 0x01, 0x00, 0x00, 0x00, 0x07};
            ASSERT_EQ(sizeof(cCorrupted), write(cWriter, cCorrupted, sizeof(cCorrupted)));
            close(cWriter);

            PollingThread.Start();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            PollingThread.Stop();

            EXPECT_EQ(0, GetReceivedCount());
        }
    }
}
