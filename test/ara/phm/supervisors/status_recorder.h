#ifndef STATUS_RECORDER_H
#define STATUS_RECORDER_H

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <vector>
#include "../../../../src/ara/phm/supervisors/elementary_supervision.h"

namespace ara
{
    namespace phm
    {
        namespace supervisors
        {
            /// @brief Thread-safe helper that records the reported supervision statuses
            class StatusRecorder
            {
            private:
                mutable std::mutex mMutex;
                std::condition_variable mConditionVariable;
                std::vector<SupervisionStatus> mStatuses;

            public:
                /// @brief Record a reported status
                /// @param status Reported supervision status
                void Record(SupervisionStatus status)
                {
                    {
                        const std::lock_guard<std::mutex> cLock{mMutex};
                        mStatuses.push_back(status);
                    }

                    mConditionVariable.notify_all();
                }

                /// @brief Wait until a certain number of statuses is recorded
                /// @param size Expected number of recorded statuses
                /// @param timeout Maximum waiting time
                /// @returns True if the expected number of statuses is recorded in time; otherwise false
                bool WaitForSize(std::size_t size, std::chrono::milliseconds timeout)
                {
                    std::unique_lock<std::mutex> _lock{mMutex};
                    return mConditionVariable.wait_for(
                        _lock, timeout, [this, size]()
                        { return mStatuses.size() >= size; });
                }

                /// @brief Wait until the latest recorded status equals a certain status
                /// @param status Expected latest status
                /// @param timeout Maximum waiting time
                /// @returns True if the expected status is recorded in time; otherwise false
                bool WaitForStatus(
                    SupervisionStatus status, std::chrono::milliseconds timeout)
                {
                    std::unique_lock<std::mutex> _lock{mMutex};
                    return mConditionVariable.wait_for(
                        _lock, timeout, [this, status]()
                        { return !mStatuses.empty() && mStatuses.back() == status; });
                }

                /// @brief Get a copy of the recorded statuses
                /// @returns Recorded statuses in the order of occurrence
                std::vector<SupervisionStatus> GetStatuses() const
                {
                    const std::lock_guard<std::mutex> cLock{mMutex};
                    return mStatuses;
                }
            };
        }
    }
}

#endif
