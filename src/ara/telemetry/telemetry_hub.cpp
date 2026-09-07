#include <chrono>
#include <json/json.h>
#include "./telemetry_hub.h"

namespace ara
{
    namespace telemetry
    {
        const std::size_t TelemetryHub::cLogCapacity;
        const std::size_t TelemetryHub::cSomeIpCapacity;
        const std::size_t TelemetryHub::cTransitionCapacity;
        const int64_t TelemetryHub::cRateWindowMs;

        TelemetryHub::TelemetryHub() : mEnabled{false},
                                       mSequence{0},
                                       mStartedAtMs{nowMs()},
                                       mSomeIpTotal{0},
                                       mGlobalSupervision{"kDeactivated", "", 0}
        {
        }

        TelemetryHub &TelemetryHub::Instance() noexcept
        {
            static TelemetryHub _instance;
            return _instance;
        }

        int64_t TelemetryHub::nowMs() noexcept
        {
            const auto cNow{std::chrono::system_clock::now().time_since_epoch()};
            const auto cMilliseconds{
                std::chrono::duration_cast<std::chrono::milliseconds>(cNow)};

            return static_cast<int64_t>(cMilliseconds.count());
        }

        uint64_t TelemetryHub::nextSequence()
        {
            return ++mSequence;
        }

        void TelemetryHub::Enable() noexcept
        {
            const std::lock_guard<std::mutex> _lock(mMutex);
            mEnabled = true;
        }

        bool TelemetryHub::Enabled() const noexcept
        {
            const std::lock_guard<std::mutex> _lock(mMutex);
            return mEnabled;
        }

        void TelemetryHub::PublishLog(LogRecord record)
        {
            const std::lock_guard<std::mutex> _lock(mMutex);

            if (!mEnabled)
            {
                return;
            }

            SequencedLog _entry{nextSequence(), nowMs(), std::move(record)};
            mLogs.push_back(std::move(_entry));

            while (mLogs.size() > cLogCapacity)
            {
                mLogs.pop_front();
            }
        }

        void TelemetryHub::PublishExecutionState(
            std::string application, std::string state)
        {
            const std::lock_guard<std::mutex> _lock(mMutex);

            if (!mEnabled)
            {
                return;
            }

            nextSequence();
            mExecutionStates[std::move(application)] =
                StateEntry{std::move(state), nowMs()};
        }

        void TelemetryHub::PublishFunctionGroupState(
            std::string functionGroup, std::string state)
        {
            const std::lock_guard<std::mutex> _lock(mMutex);

            if (!mEnabled)
            {
                return;
            }

            auto _itr{mFunctionGroupStates.find(functionGroup)};
            const bool cChanged{
                _itr == mFunctionGroupStates.end() || _itr->second.value != state};

            const int64_t cTimestampMs{nowMs()};
            const uint64_t cSequence{nextSequence()};

            if (cChanged)
            {
                SequencedTransition _transition{
                    cSequence, cTimestampMs, functionGroup, state};
                mTransitions.push_back(std::move(_transition));

                while (mTransitions.size() > cTransitionCapacity)
                {
                    mTransitions.pop_front();
                }
            }

            mFunctionGroupStates[std::move(functionGroup)] =
                StateEntry{std::move(state), cTimestampMs};
        }

        void TelemetryHub::RegisterCheckpoint(uint32_t id, std::string name)
        {
            const std::lock_guard<std::mutex> _lock(mMutex);

            if (!mEnabled)
            {
                return;
            }

            auto _itr{mCheckpoints.find(id)};
            if (_itr == mCheckpoints.end())
            {
                mCheckpoints[id] = CheckpointEntry{std::move(name), 0, 0};
            }
            else
            {
                _itr->second.name = std::move(name);
            }
        }

        void TelemetryHub::PublishCheckpoint(uint32_t id)
        {
            const std::lock_guard<std::mutex> _lock(mMutex);

            if (!mEnabled)
            {
                return;
            }

            nextSequence();

            auto _itr{mCheckpoints.find(id)};
            if (_itr == mCheckpoints.end())
            {
                mCheckpoints[id] = CheckpointEntry{"", 1, nowMs()};
            }
            else
            {
                ++_itr->second.reports;
                _itr->second.lastReportMs = nowMs();
            }
        }

        void TelemetryHub::PublishSupervisionStatus(
            std::string supervision, std::string status)
        {
            const std::lock_guard<std::mutex> _lock(mMutex);

            if (!mEnabled)
            {
                return;
            }

            auto _itr{mSupervisions.find(supervision)};
            if (_itr != mSupervisions.end() && _itr->second.status == status)
            {
                return;
            }

            nextSequence();
            mSupervisions[std::move(supervision)] =
                SupervisionEntry{std::move(status), "", nowMs()};
        }

        void TelemetryHub::PublishGlobalSupervisionStatus(
            std::string status, std::string dominantType)
        {
            const std::lock_guard<std::mutex> _lock(mMutex);

            if (!mEnabled)
            {
                return;
            }

            if (mGlobalSupervision.status == status &&
                mGlobalSupervision.dominantType == dominantType)
            {
                return;
            }

            nextSequence();
            mGlobalSupervision =
                SupervisionEntry{
                    std::move(status), std::move(dominantType), nowMs()};
        }

        void TelemetryHub::PublishSomeIpMessage(SomeIpRecord record)
        {
            const std::lock_guard<std::mutex> _lock(mMutex);

            if (!mEnabled)
            {
                return;
            }

            const int64_t cTimestampMs{nowMs()};

            SequencedSomeIp _entry{nextSequence(), cTimestampMs, std::move(record)};
            mSomeIpMessages.push_back(std::move(_entry));
            ++mSomeIpTotal;

            while (mSomeIpMessages.size() > cSomeIpCapacity)
            {
                mSomeIpMessages.pop_front();
            }

            mSomeIpTimestamps.push_back(cTimestampMs);
            while (!mSomeIpTimestamps.empty() &&
                   mSomeIpTimestamps.front() < cTimestampMs - cRateWindowMs)
            {
                mSomeIpTimestamps.pop_front();
            }
        }

        double TelemetryHub::someIpRate(int64_t nowMs) const
        {
            std::size_t _count{0};
            for (const int64_t cTimestampMs : mSomeIpTimestamps)
            {
                if (cTimestampMs >= nowMs - cRateWindowMs)
                {
                    ++_count;
                }
            }

            const double cWindowSeconds{
                static_cast<double>(cRateWindowMs) / 1000.0};

            return static_cast<double>(_count) / cWindowSeconds;
        }

        uint64_t TelemetryHub::Sequence() const
        {
            const std::lock_guard<std::mutex> _lock(mMutex);
            return mSequence;
        }

        std::string TelemetryHub::SerializeSnapshot(uint64_t sinceSequence) const
        {
            const std::lock_guard<std::mutex> _lock(mMutex);

            const int64_t cTimestampMs{nowMs()};

            Json::Value _root(Json::objectValue);
            _root["sequence"] = static_cast<Json::UInt64>(mSequence);
            _root["generatedAtMs"] = static_cast<Json::Int64>(cTimestampMs);
            _root["uptimeMs"] =
                static_cast<Json::Int64>(cTimestampMs - mStartedAtMs);

            Json::Value _applications(Json::arrayValue);
            for (const auto &cEntry : mExecutionStates)
            {
                Json::Value _application(Json::objectValue);
                _application["name"] = cEntry.first;
                _application["executionState"] = cEntry.second.value;
                _application["updatedAtMs"] =
                    static_cast<Json::Int64>(cEntry.second.updatedAtMs);
                _applications.append(_application);
            }
            _root["applications"] = _applications;

            Json::Value _functionGroups(Json::arrayValue);
            for (const auto &cEntry : mFunctionGroupStates)
            {
                Json::Value _functionGroup(Json::objectValue);
                _functionGroup["name"] = cEntry.first;
                _functionGroup["state"] = cEntry.second.value;
                _functionGroup["updatedAtMs"] =
                    static_cast<Json::Int64>(cEntry.second.updatedAtMs);
                _functionGroups.append(_functionGroup);
            }
            _root["functionGroups"] = _functionGroups;

            Json::Value _transitions(Json::arrayValue);
            for (const auto &cEntry : mTransitions)
            {
                if (cEntry.sequence <= sinceSequence)
                {
                    continue;
                }

                Json::Value _transition(Json::objectValue);
                _transition["sequence"] = static_cast<Json::UInt64>(cEntry.sequence);
                _transition["timestampMs"] =
                    static_cast<Json::Int64>(cEntry.timestampMs);
                _transition["functionGroup"] = cEntry.functionGroup;
                _transition["state"] = cEntry.state;
                _transitions.append(_transition);
            }
            _root["transitions"] = _transitions;

            Json::Value _checkpoints(Json::arrayValue);
            for (const auto &cEntry : mCheckpoints)
            {
                Json::Value _checkpoint(Json::objectValue);
                _checkpoint["id"] = static_cast<Json::UInt>(cEntry.first);
                _checkpoint["name"] = cEntry.second.name;
                _checkpoint["reports"] =
                    static_cast<Json::UInt64>(cEntry.second.reports);
                _checkpoint["lastReportMs"] =
                    static_cast<Json::Int64>(cEntry.second.lastReportMs);
                _checkpoints.append(_checkpoint);
            }
            _root["checkpoints"] = _checkpoints;

            Json::Value _supervisions(Json::arrayValue);
            for (const auto &cEntry : mSupervisions)
            {
                Json::Value _supervision(Json::objectValue);
                _supervision["name"] = cEntry.first;
                _supervision["status"] = cEntry.second.status;
                _supervision["updatedAtMs"] =
                    static_cast<Json::Int64>(cEntry.second.updatedAtMs);
                _supervisions.append(_supervision);
            }
            _root["supervisions"] = _supervisions;

            Json::Value _globalSupervision(Json::objectValue);
            _globalSupervision["status"] = mGlobalSupervision.status;
            _globalSupervision["dominantType"] = mGlobalSupervision.dominantType;
            _globalSupervision["updatedAtMs"] =
                static_cast<Json::Int64>(mGlobalSupervision.updatedAtMs);
            _root["globalSupervision"] = _globalSupervision;

            Json::Value _someIpMessages(Json::arrayValue);
            for (const auto &cEntry : mSomeIpMessages)
            {
                if (cEntry.sequence <= sinceSequence)
                {
                    continue;
                }

                Json::Value _message(Json::objectValue);
                _message["sequence"] = static_cast<Json::UInt64>(cEntry.sequence);
                _message["timestampMs"] =
                    static_cast<Json::Int64>(cEntry.timestampMs);
                _message["direction"] = cEntry.record.direction;
                _message["role"] = cEntry.record.role;
                _message["serviceId"] =
                    static_cast<Json::UInt>(cEntry.record.serviceId);
                _message["methodId"] =
                    static_cast<Json::UInt>(cEntry.record.methodId);
                _message["payloadLength"] =
                    static_cast<Json::UInt>(cEntry.record.payloadLength);
                _someIpMessages.append(_message);
            }

            Json::Value _someIp(Json::objectValue);
            _someIp["total"] = static_cast<Json::UInt64>(mSomeIpTotal);
            _someIp["ratePerSecond"] = someIpRate(cTimestampMs);
            _someIp["messages"] = _someIpMessages;
            _root["someip"] = _someIp;

            Json::Value _logs(Json::arrayValue);
            for (const auto &cEntry : mLogs)
            {
                if (cEntry.sequence <= sinceSequence)
                {
                    continue;
                }

                Json::Value _log(Json::objectValue);
                _log["sequence"] = static_cast<Json::UInt64>(cEntry.sequence);
                _log["timestampMs"] =
                    static_cast<Json::Int64>(cEntry.timestampMs);
                _log["application"] = cEntry.record.application;
                _log["context"] = cEntry.record.context;
                _log["level"] = cEntry.record.level;
                _log["message"] = cEntry.record.message;
                _logs.append(_log);
            }
            _root["logs"] = _logs;

            Json::StreamWriterBuilder _builder;
            _builder["indentation"] = "";

            return Json::writeString(_builder, _root);
        }

        void TelemetryHub::Reset()
        {
            const std::lock_guard<std::mutex> _lock(mMutex);

            mSequence = 0;
            mSomeIpTotal = 0;
            mStartedAtMs = nowMs();
            mGlobalSupervision = SupervisionEntry{"kDeactivated", "", 0};
            mExecutionStates.clear();
            mFunctionGroupStates.clear();
            mCheckpoints.clear();
            mSupervisions.clear();
            mLogs.clear();
            mSomeIpMessages.clear();
            mTransitions.clear();
            mSomeIpTimestamps.clear();
        }
    }
}
