#include <array>
#include <chrono>
#include <fcntl.h>
#include <functional>
#include <thread>
#include <unistd.h>
#include <sys/stat.h>
#include <gtest/gtest.h>
#include "../../../src/ara/com/helper/payload_helper.h"
#include "../../../src/application/helper/fifo_checkpoint_communicator.h"

namespace application
{
    namespace helper
    {
        class FifoCheckpointCommunicatorTest : public testing::Test
        {
        private:
            static const int cMaxPollIterations{200};
            static const int cPollTimeoutMs{10};

        protected:
            AsyncBsdSocketLib::Poller Poller;
            std::string FifoPath;

            void SetUp() override
            {
                char _template[]{"/tmp/fifo_checkpoint_test_XXXXXX"};
                ASSERT_NE(nullptr, mkdtemp(_template));
                FifoPath = std::string(_template) + "/checkpoint.fifo";
            }

            void TearDown() override
            {
                unlink(FifoPath.c_str());
                const std::size_t cSeparator{FifoPath.find_last_of('/')};
                rmdir(FifoPath.substr(0, cSeparator).c_str());
            }

            bool PollUntil(std::function<bool()> predicate)
            {
                for (int i = 0; i < cMaxPollIterations; ++i)
                {
                    if (predicate())
                    {
                        return true;
                    }

                    Poller.TryPoll(cPollTimeoutMs);
                }

                return false;
            }

            void PollFor(int iterations)
            {
                for (int i = 0; i < iterations; ++i)
                {
                    Poller.TryPoll(cPollTimeoutMs);
                }
            }

            void WriteRaw(const std::vector<uint8_t> &payload)
            {
                const int cFd{open(FifoPath.c_str(), O_WRONLY | O_NONBLOCK)};
                ASSERT_GE(cFd, 0);
                const ssize_t cWritten{write(cFd, payload.data(), payload.size())};
                close(cFd);
                ASSERT_EQ(static_cast<ssize_t>(payload.size()), cWritten);
            }
        };

        TEST_F(FifoCheckpointCommunicatorTest, Constructor)
        {
            EXPECT_NO_THROW(FifoCheckpointCommunicator _communicator(&Poller, FifoPath));

            struct stat _stat;
            ASSERT_EQ(0, stat(FifoPath.c_str(), &_stat));
            EXPECT_TRUE(S_ISFIFO(_stat.st_mode));
        }

        TEST_F(FifoCheckpointCommunicatorTest, InvalidPathConstructor)
        {
            const std::string cInvalidPath{"/nonexistent_directory/checkpoint.fifo"};
            EXPECT_THROW(
                FifoCheckpointCommunicator _communicator(&Poller, cInvalidPath),
                std::runtime_error);
        }

        TEST_F(FifoCheckpointCommunicatorTest, TrySendRoundTrip)
        {
            const uint32_t cCheckpoint{0x12345678};

            FifoCheckpointCommunicator _communicator(&Poller, FifoPath);

            bool _received{false};
            uint32_t _receivedCheckpoint{0};
            _communicator.SetCallback(
                [&](uint32_t checkpoint)
                {
                    _received = true;
                    _receivedCheckpoint = checkpoint;
                });

            EXPECT_TRUE(_communicator.TrySend(cCheckpoint));
            ASSERT_TRUE(PollUntil([&]()
                                  { return _received; }));
            EXPECT_EQ(cCheckpoint, _receivedCheckpoint);
        }

        TEST_F(FifoCheckpointCommunicatorTest, TrySendSequentialRoundTrips)
        {
            const std::array<uint32_t, 3> cCheckpoints{1, 2, 0xdeadbeef};

            FifoCheckpointCommunicator _communicator(&Poller, FifoPath);

            std::vector<uint32_t> _receivedCheckpoints;
            _communicator.SetCallback(
                [&](uint32_t checkpoint)
                { _receivedCheckpoints.push_back(checkpoint); });

            for (std::size_t i = 0; i < cCheckpoints.size(); ++i)
            {
                ASSERT_TRUE(_communicator.TrySend(cCheckpoints[i]));
                ASSERT_TRUE(PollUntil([&]()
                                      { return _receivedCheckpoints.size() == i + 1; }));
            }

            const std::vector<uint32_t> cExpected(
                cCheckpoints.cbegin(), cCheckpoints.cend());
            EXPECT_EQ(cExpected, _receivedCheckpoints);
        }

        TEST_F(FifoCheckpointCommunicatorTest, ReceiveProfile11ProtectedPayload)
        {
            const uint32_t cCheckpoint{0x0a0b0c0d};

            FifoCheckpointCommunicator _communicator(&Poller, FifoPath);

            bool _received{false};
            uint32_t _receivedCheckpoint{0};
            _communicator.SetCallback(
                [&](uint32_t checkpoint)
                {
                    _received = true;
                    _receivedCheckpoint = checkpoint;
                });

            // Wire format: 1 byte CRC + 1 byte counter + 4 bytes big-endian checkpoint
            std::vector<uint8_t> _unprotected;
            ara::com::helper::Inject(_unprotected, cCheckpoint);
            ara::com::e2e::Profile11 _profile;
            std::vector<uint8_t> _protected;
            ASSERT_TRUE(_profile.TryProtect(_unprotected, _protected));
            ASSERT_EQ(6, _protected.size());

            WriteRaw(_protected);

            ASSERT_TRUE(PollUntil([&]()
                                  { return _received; }));
            EXPECT_EQ(cCheckpoint, _receivedCheckpoint);
        }

        TEST_F(FifoCheckpointCommunicatorTest, DiscardCorruptedCrc)
        {
            const uint32_t cCheckpoint{0x0a0b0c0d};
            const int cPollIterations{20};

            FifoCheckpointCommunicator _communicator(&Poller, FifoPath);

            bool _received{false};
            _communicator.SetCallback(
                [&](uint32_t checkpoint)
                { _received = true; });

            std::vector<uint8_t> _unprotected;
            ara::com::helper::Inject(_unprotected, cCheckpoint);
            ara::com::e2e::Profile11 _profile;
            std::vector<uint8_t> _protected;
            ASSERT_TRUE(_profile.TryProtect(_unprotected, _protected));

            const std::size_t cCrcIndex{0};
            _protected[cCrcIndex] ^= 0xff;

            WriteRaw(_protected);
            PollFor(cPollIterations);

            EXPECT_FALSE(_received);
        }
    }
}
