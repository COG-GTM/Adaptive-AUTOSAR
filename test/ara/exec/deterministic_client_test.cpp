#include <algorithm>
#include <random>
#include <vector>
#include <gtest/gtest.h>
#include "../../../src/ara/exec/deterministic_client.h"

namespace ara
{
    namespace exec
    {
        TEST(DeterministicClientTest, GetRandomMethod)
        {
            DeterministicClient _deterministicClient;
            DeterministicClient _otherDeterministicClient;

            uint64_t _random = _deterministicClient.GetRandom();
            uint64_t _otherRandom = _otherDeterministicClient.GetRandom();
            EXPECT_EQ(_random, _otherRandom);

            _deterministicClient.WaitForActivation();

            _random = _deterministicClient.GetRandom();
            EXPECT_NE(_random, _otherRandom);
        }

        TEST(DeterministicClientTest, TimeStamps)
        {
            DeterministicClient _deterministicClient;

            auto _activationTime = _deterministicClient.GetActivationTime();
            auto _nextActivationTime = _deterministicClient.GetNextActivationTime();
            EXPECT_GT(_nextActivationTime.Value(), _activationTime.Value());

            _deterministicClient.WaitForActivation();
            _activationTime = _deterministicClient.GetActivationTime();
            EXPECT_GE(_activationTime.Value(), _nextActivationTime.Value());
        }

        TEST(DeterministicClientTest, WaitForActivationMethod)
        {
            const ActivationReturnType cExpectedResult{ActivationReturnType::kRun};
            DeterministicClient _deterministicClient;

            core::Result<ActivationReturnType> _result{
                _deterministicClient.WaitForActivation()};

            EXPECT_TRUE(_result.HasValue());
            EXPECT_EQ(cExpectedResult, _result.Value());
        }

        TEST(DeterministicClientTest, SetRandomSeedMethod)
        {
            const uint64_t cSeed{12345};
            // The cycle keeps running while the test reads the random number,
            // so accept any of the first numbers of the seeded sequence.
            const size_t cSequenceLength{16};

            std::default_random_engine _generator;
            _generator.seed(cSeed);
            std::uniform_int_distribution<uint64_t> _distribution;
            std::vector<uint64_t> _expectedSequence;
            for (size_t i = 0; i < cSequenceLength; ++i)
            {
                _expectedSequence.push_back(_distribution(_generator));
            }

            DeterministicClient _deterministicClient;
            _deterministicClient.SetRandomSeed(cSeed);
            // The seed is applied at the beginning of the next cycle,
            // so two activations guarantee that it has been consumed.
            _deterministicClient.WaitForActivation();
            _deterministicClient.WaitForActivation();
            uint64_t _actualResult = _deterministicClient.GetRandom();

            bool _found{
                std::find(
                    _expectedSequence.cbegin(),
                    _expectedSequence.cend(),
                    _actualResult) != _expectedSequence.cend()};
            EXPECT_TRUE(_found);
        }

        TEST(DeterministicClientTest, SharedCycleAmongInstances)
        {
            DeterministicClient _deterministicClient;
            _deterministicClient.WaitForActivation();

            {
                // Disposing a non-last instance should not stop the cycle
                DeterministicClient _otherDeterministicClient;
            }

            uint64_t _random = _deterministicClient.GetRandom();
            _deterministicClient.WaitForActivation();
            _deterministicClient.WaitForActivation();
            uint64_t _nextRandom = _deterministicClient.GetRandom();

            EXPECT_NE(_random, _nextRandom);
        }

        TEST(DeterministicClientTest, NextActivationTimeOffset)
        {
            const std::chrono::milliseconds cCycleDuration{10};
            DeterministicClient _deterministicClient;
            _deterministicClient.WaitForActivation();

            // The cycle may advance between the two reads,
            // so retry until both reads belong to the same cycle.
            const int cMaxRetries{100};
            bool _consistent{false};
            for (int i = 0; i < cMaxRetries && !_consistent; ++i)
            {
                DeterministicClient::TimeStamp _activationTime{
                    _deterministicClient.GetActivationTime().Value()};
                DeterministicClient::TimeStamp _nextActivationTime{
                    _deterministicClient.GetNextActivationTime().Value()};
                DeterministicClient::TimeStamp _activationTimeAgain{
                    _deterministicClient.GetActivationTime().Value()};

                if (_activationTime == _activationTimeAgain)
                {
                    _consistent = true;
                    EXPECT_EQ(_activationTime + cCycleDuration, _nextActivationTime);
                }
            }

            EXPECT_TRUE(_consistent);
        }
    }
}