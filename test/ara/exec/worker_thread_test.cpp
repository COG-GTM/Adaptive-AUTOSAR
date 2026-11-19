#include <set>
#include <thread>
#include <vector>
#include <gtest/gtest.h>
#include "../../../src/ara/exec/worker_thread.h"

namespace ara
{
    namespace exec
    {
        TEST(WorkerThreadTest, SingleInstanceUniqueness)
        {
            WorkerThread _workerThread;

            uint64_t _random1 = _workerThread.GetRandom();
            uint64_t _random2 = _workerThread.GetRandom();

            uint64_t _diff = _random2 - _random1;
            EXPECT_GE(_diff, WorkerThread::cOffsetStep);
        }

        TEST(WorkerThreadTest, MultiInstanceUniqueness)
        {
            WorkerThread _workerThread1;
            WorkerThread _workerThread2;

            uint64_t _random1 = _workerThread1.GetRandom();
            uint64_t _random2 = _workerThread2.GetRandom();

            uint64_t _diff = _random2 - _random1;
            EXPECT_GE(_diff, WorkerThread::cOffsetStep);
        }

        TEST(WorkerThreadTest, SequentialUniqueness)
        {
            const size_t cCount{1000};
            WorkerThread _workerThread;
            WorkerThread _otherWorkerThread;

            std::set<uint64_t> _randoms;
            for (size_t i = 0; i < cCount; ++i)
            {
                _randoms.insert(_workerThread.GetRandom());
                _randoms.insert(_otherWorkerThread.GetRandom());
            }

            EXPECT_EQ(2 * cCount, _randoms.size());
        }

        // Documents a suspected production bug (not fixed here on purpose):
        // WorkerThread::GetRandom reads the shared offset and advances it in two
        // separate atomic operations, so two threads can use the same offset
        // window and return the same 'unique' random number.
        // Enable the test once the offset is fetched and advanced atomically.
        TEST(WorkerThreadTest, DISABLED_BugConcurrentUniqueness)
        {
            const size_t cThreadCount{4};
            const size_t cCount{200000};

            std::vector<std::vector<uint64_t>> _randoms(cThreadCount);
            std::vector<std::thread> _threads;
            for (size_t i = 0; i < cThreadCount; ++i)
            {
                std::vector<uint64_t> &_threadRandoms = _randoms[i];
                _threads.emplace_back(
                    [&_threadRandoms, cCount]()
                    {
                        WorkerThread _workerThread;
                        _threadRandoms.reserve(cCount);
                        for (size_t j = 0; j < cCount; ++j)
                        {
                            _threadRandoms.push_back(_workerThread.GetRandom());
                        }
                    });
            }

            std::set<uint64_t> _uniqueRandoms;
            for (size_t i = 0; i < cThreadCount; ++i)
            {
                _threads[i].join();
                _uniqueRandoms.insert(_randoms[i].cbegin(), _randoms[i].cend());
            }

            EXPECT_EQ(cThreadCount * cCount, _uniqueRandoms.size());
        }
    }
}