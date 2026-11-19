#include <gtest/gtest.h>
#include "../../../src/ara/exec/execution_client.h"
#include "./helper/mock_rpc_client.h"

namespace ara
{
    namespace exec
    {
        class ExecutionClientTest : public testing::Test
        {
        private:
            static const int64_t cTimeout{2};
            const core::InstanceSpecifier cInstance{"test_instance"};
            helper::MockRpcClient mRpcClient;

        protected:
            ExecutionClient Client;

            ExecutionClientTest() : Client{cInstance, &mRpcClient, cTimeout}
            {
            }

            void SetBypass(bool bypass)
            {
                mRpcClient.SetBypass(bypass);
            }

            void SetRpcPayload(std::vector<uint8_t> &&rpcPayload)
            {
                mRpcClient.SetRpcPayload(std::move(rpcPayload));
            }
        };

        const int64_t ExecutionClientTest::cTimeout;

        TEST_F(ExecutionClientTest, ReportExecutionStateMethod)
        {
            const ExecutionState cState{ExecutionState::kRunning};
            core::Result<void> _result{Client.ReportExecutionState(cState)};
            EXPECT_TRUE(_result.HasValue());
        }

        TEST_F(ExecutionClientTest, RedundantRequestScenario)
        {
            const ExecutionState cState{ExecutionState::kRunning};

            core::Result<void> _result{Client.ReportExecutionState(cState)};
            EXPECT_TRUE(_result.HasValue());

            core::Result<void> _redundantResult{Client.ReportExecutionState(cState)};
            EXPECT_TRUE(_redundantResult.HasValue());
        }

        TEST_F(ExecutionClientTest, TimeoutScenario)
        {
            const ExecErrc cExpectedResult{ExecErrc::kCommunicationError};
            const ExecutionState cState{ExecutionState::kRunning};

            SetBypass(true);
            core::Result<void> _result{Client.ReportExecutionState(cState)};

            EXPECT_FALSE(_result.HasValue());
            auto _actualResult{static_cast<ExecErrc>(_result.Error().Value())};
            EXPECT_EQ(cExpectedResult, _actualResult);
        }

        TEST_F(ExecutionClientTest, CorruptedResponseScenario)
        {
            const ExecErrc cExpectedResult{ExecErrc::kCommunicationError};
            const ExecutionState cState{ExecutionState::kRunning};

            SetRpcPayload(std::vector<uint8_t>({0x00}));
            core::Result<void> _result{Client.ReportExecutionState(cState)};

            EXPECT_FALSE(_result.HasValue());
            auto _actualResult{static_cast<ExecErrc>(_result.Error().Value())};
            EXPECT_EQ(cExpectedResult, _actualResult);
        }

        TEST_F(ExecutionClientTest, RequestFailureScenario)
        {
            const ExecErrc cExpectedResult{ExecErrc::kGeneralError};
            const ExecutionState cState{ExecutionState::kRunning};

            std::vector<uint8_t> _rcpPayload;
            auto _errorCodeInt{static_cast<uint32_t>(cExpectedResult)};
            com::helper::Inject(_rcpPayload, _errorCodeInt);
            SetRpcPayload(std::move(_rcpPayload));
            core::Result<void> _result{Client.ReportExecutionState(cState)};

            EXPECT_FALSE(_result.HasValue());
            auto _actualResult{static_cast<ExecErrc>(_result.Error().Value())};
            EXPECT_EQ(cExpectedResult, _actualResult);
        }

        TEST(ExecutionClientCtorTest, InvalidTimeoutConstruction)
        {
            const core::InstanceSpecifier cInstance{"test_instance"};
            const int64_t cZeroTimeout{0};
            const int64_t cNegativeTimeout{-1};
            helper::MockRpcClient _rpcClient;

            EXPECT_THROW(
                ExecutionClient _client(cInstance, &_rpcClient, cZeroTimeout),
                std::invalid_argument);
            EXPECT_THROW(
                ExecutionClient _client(cInstance, &_rpcClient, cNegativeTimeout),
                std::invalid_argument);
        }

        TEST(ExecutionClientCtorTest, ReportAfterTimeoutScenario)
        {
            const core::InstanceSpecifier cInstance{"test_instance"};
            const int64_t cTimeout{1};
            const ExecErrc cExpectedTimeoutResult{ExecErrc::kCommunicationError};
            const ExecErrc cExpectedPendingResult{ExecErrc::kFailed};
            const ExecutionState cState{ExecutionState::kRunning};

            helper::MockRpcClient _rpcClient;
            ExecutionClient _client(cInstance, &_rpcClient, cTimeout);

            _rpcClient.SetBypass(true);
            core::Result<void> _timeoutResult{_client.ReportExecutionState(cState)};
            EXPECT_FALSE(_timeoutResult.HasValue());
            ExecErrc _actualTimeoutResult{
                static_cast<ExecErrc>(_timeoutResult.Error().Value())};
            EXPECT_EQ(cExpectedTimeoutResult, _actualTimeoutResult);

            // The timed out request is still pending, so a new report is rejected
            // even though the server would respond this time.
            _rpcClient.SetBypass(false);
            core::Result<void> _pendingResult{_client.ReportExecutionState(cState)};
            EXPECT_FALSE(_pendingResult.HasValue());
            ExecErrc _actualPendingResult{
                static_cast<ExecErrc>(_pendingResult.Error().Value())};
            EXPECT_EQ(cExpectedPendingResult, _actualPendingResult);
        }

        TEST_F(ExecutionClientTest, ErrorDomainOfFailedRequest)
        {
            const ExecErrorDomain cExpectedDomain;
            const ExecErrc cErrorCode{ExecErrc::kAlreadyInState};
            const ExecutionState cState{ExecutionState::kRunning};

            std::vector<uint8_t> _rpcPayload;
            uint32_t _errorCodeInt{static_cast<uint32_t>(cErrorCode)};
            com::helper::Inject(_rpcPayload, _errorCodeInt);
            SetRpcPayload(std::move(_rpcPayload));
            core::Result<void> _result{Client.ReportExecutionState(cState)};

            EXPECT_FALSE(_result.HasValue());
            EXPECT_TRUE(_result.Error().Domain() == cExpectedDomain);
            ExecErrc _actualResult{static_cast<ExecErrc>(_result.Error().Value())};
            EXPECT_EQ(cErrorCode, _actualResult);
        }

        TEST_F(ExecutionClientTest, ReportAfterFailureScenario)
        {
            const ExecErrc cErrorCode{ExecErrc::kGeneralError};
            const ExecutionState cState{ExecutionState::kRunning};

            std::vector<uint8_t> _rpcPayload;
            uint32_t _errorCodeInt{static_cast<uint32_t>(cErrorCode)};
            com::helper::Inject(_rpcPayload, _errorCodeInt);
            SetRpcPayload(std::move(_rpcPayload));
            core::Result<void> _failedResult{Client.ReportExecutionState(cState)};
            EXPECT_FALSE(_failedResult.HasValue());

            // A responded (even though failed) request does not block the next one.
            SetRpcPayload(std::vector<uint8_t>());
            core::Result<void> _result{Client.ReportExecutionState(cState)};
            EXPECT_TRUE(_result.HasValue());
        }
    }
}