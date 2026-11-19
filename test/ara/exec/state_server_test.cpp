#include <gtest/gtest.h>
#include "../../../src/ara/exec/state_server.h"
#include "./helper/mock_rpc_server.h"

namespace ara
{
    namespace exec
    {
        class StateServerTest : public testing::Test
        {
        private:
            const uint16_t cClientId{0x0004};

            helper::MockRpcServer mRpcServer;
            uint16_t mSessionId;

        protected:
            const uint32_t cSetStateMessageId{0x00030001};
            const uint32_t cStateTransitionMessageId{0x00030002};

            StateServer Server;

            StateServerTest() : mRpcServer(cProtocolVersion, cInterfaceVersion),
                                Server{&mRpcServer, {{"MachineFG", "Off"}, {"MachineFG", "StartUp"}}, {{"MachineFG", "Off"}}},
                                mSessionId{0}
            {
            }

            com::someip::rpc::SomeIpRpcMessage Send(
                uint32_t messageId,
                std::vector<uint8_t> &&rpcPayload)
            {
                com::someip::rpc::SomeIpRpcMessage _request(
                    messageId,
                    cClientId,
                    ++mSessionId,
                    cProtocolVersion,
                    cInterfaceVersion,
                    std::move(rpcPayload));

                auto _result{mRpcServer.Send(_request)};

                return _result;
            }

        public:
            static const uint8_t cProtocolVersion{1};
            static const uint8_t cInterfaceVersion{1};
        };

        const uint8_t StateServerTest::cProtocolVersion;
        const uint8_t StateServerTest::cInterfaceVersion;

        TEST(StateServerCtorTest, InvalidArgumentConstruction)
        {
            helper::MockRpcServer _rpcServer(
                StateServerTest::cProtocolVersion,
                StateServerTest::cInterfaceVersion);

            EXPECT_THROW(
                StateServer _server(&_rpcServer, {{"MachineFG", "StartUp"}}, {{"MachineFG", "Off"}}),
                std::invalid_argument);
        }

        TEST(StateServerCtorTest, IllogicalConstruction)
        {
            helper::MockRpcServer _rpcServer(
                StateServerTest::cProtocolVersion,
                StateServerTest::cInterfaceVersion);

            EXPECT_THROW(
                StateServer _server(&_rpcServer, {{"MachineFG", "StartUp"}}, {}),
                std::logic_error);
        }

        TEST_F(StateServerTest, TryGetStateMethod)
        {
            const std::string cFunctionGroup{"FunctionGroup"};
            std::string _state;
            EXPECT_FALSE(Server.TryGetState(cFunctionGroup, _state));
        }

        TEST_F(StateServerTest, SetStateScenario)
        {
            const auto cExpectedReturnCode{com::someip::SomeIpReturnCode::eOK};
            const std::string cFunctionGroup{"MachineFG"};
            const auto cExpectedState{"StartUp"};

            std::vector<uint8_t> _rpcPayload({0, 0, 0, 9,
                                              77, 97, 99, 104, 105, 110, 101, 70, 71,
                                              0, 0, 0, 7,
                                              83, 116, 97, 114, 116, 85, 112});
            auto _response{Send(cSetStateMessageId, std::move(_rpcPayload))};

            com::someip::SomeIpReturnCode _acutalReturnCode{_response.ReturnCode()};
            EXPECT_EQ(cExpectedReturnCode, _acutalReturnCode);

            std::string _actualState;
            EXPECT_TRUE(Server.TryGetState(cFunctionGroup, _actualState));
            EXPECT_EQ(cExpectedState, _actualState);
        }

        TEST_F(StateServerTest, EmptyRpcPayloadScenario)
        {
            const ExecErrc cExpectedResult{ExecErrc::kInvalidArguments};

            auto _response{Send(cSetStateMessageId, std::vector<uint8_t>())};

            ExecErrc _actualResult;
            EXPECT_TRUE(
                helper::MockRpcServer::TryGetErrorCode(_response, _actualResult));
            EXPECT_EQ(cExpectedResult, _actualResult);
        }

        TEST_F(StateServerTest, ShortRpcPayloadScenario)
        {
            const ExecErrc cExpectedResult{ExecErrc::kInvalidArguments};

            std::vector<uint8_t> _rpcPayload({0, 0, 0, 9,
                                              77, 97, 99, 104, 105, 110, 101, 70, 71});
            auto _response{Send(cSetStateMessageId, std::move(_rpcPayload))};

            ExecErrc _actualResult;
            EXPECT_TRUE(
                helper::MockRpcServer::TryGetErrorCode(_response, _actualResult));
            EXPECT_EQ(cExpectedResult, _actualResult);
        }

        TEST_F(StateServerTest, NoStatePayloadScenario)
        {
            const ExecErrc cExpectedResult{ExecErrc::kInvalidArguments};

            std::vector<uint8_t> _rpcPayload({0, 0, 0, 9,
                                              77, 97, 99, 104, 105, 110, 101, 70, 71,
                                              0, 0, 0, 4});
            auto _response{Send(cSetStateMessageId, std::move(_rpcPayload))};

            ExecErrc _actualResult;
            EXPECT_TRUE(
                helper::MockRpcServer::TryGetErrorCode(_response, _actualResult));
            EXPECT_EQ(cExpectedResult, _actualResult);
        }

        TEST_F(StateServerTest, InvalidTransitionScenario)
        {
            const ExecErrc cExpectedResult{ExecErrc::kInvalidTransition};

            std::vector<uint8_t> _rpcPayload({0, 0, 0, 9,
                                              77, 97, 99, 104, 105, 110, 101, 70, 71,
                                              0, 0, 0, 4,
                                              68, 105, 97, 103});
            auto _response{Send(cSetStateMessageId, std::move(_rpcPayload))};

            ExecErrc _actualResult;
            EXPECT_TRUE(
                helper::MockRpcServer::TryGetErrorCode(_response, _actualResult));
            EXPECT_EQ(cExpectedResult, _actualResult);
        }

        TEST_F(StateServerTest, InvalidStateScenario)
        {
            const ExecErrc cExpectedResult{ExecErrc::kAlreadyInState};

            std::vector<uint8_t> _rpcPayload({0, 0, 0, 9,
                                              77, 97, 99, 104, 105, 110, 101, 70, 71,
                                              0, 0, 0, 3,
                                              79, 102, 102});
            auto _response{Send(cSetStateMessageId, std::move(_rpcPayload))};

            ExecErrc _actualResult;
            EXPECT_TRUE(
                helper::MockRpcServer::TryGetErrorCode(_response, _actualResult));
            EXPECT_EQ(cExpectedResult, _actualResult);
        }

        TEST_F(StateServerTest, InitializationSenario)
        {
            const auto cExpectedReturnCode{com::someip::SomeIpReturnCode::eOK};
            const auto cExpectedErrorCode{ExecErrc::kFailed};

            EXPECT_FALSE(Server.Initialized());

            auto _response{Send(cStateTransitionMessageId, {})};
            com::someip::SomeIpReturnCode _actualReturnCode{_response.ReturnCode()};
            EXPECT_EQ(cExpectedReturnCode, _actualReturnCode);

            EXPECT_TRUE(Server.Initialized());

            _response = Send(cStateTransitionMessageId, {});
            ExecErrc _actualErrorCode;
            EXPECT_TRUE(
                helper::MockRpcServer::TryGetErrorCode(_response, _actualErrorCode));
            EXPECT_EQ(cExpectedErrorCode, _actualErrorCode);

            EXPECT_TRUE(Server.Initialized());
        }

        TEST_F(StateServerTest, InvalidInitialization)
        {
            const auto cExpectedResult{ExecErrc::kInvalidArguments};
            const uint8_t cInvalidPayloadByte{0x00};

            auto _response{Send(cStateTransitionMessageId, {cInvalidPayloadByte})};
            ExecErrc _actualResult;
            EXPECT_TRUE(
                helper::MockRpcServer::TryGetErrorCode(_response, _actualResult));
            EXPECT_EQ(cExpectedResult, _actualResult);

            EXPECT_FALSE(Server.Initialized());
        }

        TEST_F(StateServerTest, SetNotifierMethod)
        {
            const std::string cFunctionGroup{"MachineFG"};

            bool _notified{false};
            auto _callback{[&]()
                           { _notified = true; }};
            Server.SetNotifier(cFunctionGroup, _callback);

            std::vector<uint8_t>
                _rpcPayload({0, 0, 0, 9,
                             77, 97, 99, 104, 105, 110, 101, 70, 71,
                             0, 0, 0, 7,
                             83, 116, 97, 114, 116, 85, 112});
            auto _response{Send(cSetStateMessageId, std::move(_rpcPayload))};

            EXPECT_TRUE(_notified);
        }

        static std::vector<uint8_t> makeSetStatePayload(
            const std::string &functionGroup, const std::string &state)
        {
            std::vector<uint8_t> _result;
            com::helper::Inject(
                _result, static_cast<uint32_t>(functionGroup.length()));
            _result.insert(_result.end(), functionGroup.cbegin(), functionGroup.cend());
            com::helper::Inject(
                _result, static_cast<uint32_t>(state.length()));
            _result.insert(_result.end(), state.cbegin(), state.cend());

            return _result;
        }

        TEST(StateServerCtorTest, MultipleFunctionGroupsConstruction)
        {
            const std::string cMachineFunctionGroup{"MachineFG"};
            const std::string cOtherFunctionGroup{"OtherFG"};
            const std::string cExpectedMachineState{"Off"};
            const std::string cExpectedOtherState{"Idle"};

            helper::MockRpcServer _rpcServer(
                StateServerTest::cProtocolVersion,
                StateServerTest::cInterfaceVersion);
            StateServer _server(
                &_rpcServer,
                {{"MachineFG", "Off"}, {"MachineFG", "StartUp"}, {"OtherFG", "Idle"}},
                {{"MachineFG", "Off"}, {"OtherFG", "Idle"}});

            std::string _actualState;
            EXPECT_TRUE(_server.TryGetState(cMachineFunctionGroup, _actualState));
            EXPECT_EQ(cExpectedMachineState, _actualState);
            EXPECT_TRUE(_server.TryGetState(cOtherFunctionGroup, _actualState));
            EXPECT_EQ(cExpectedOtherState, _actualState);
            EXPECT_FALSE(_server.Initialized());
        }

        TEST(StateServerCtorTest, MissingOneInitialStateConstruction)
        {
            helper::MockRpcServer _rpcServer(
                StateServerTest::cProtocolVersion,
                StateServerTest::cInterfaceVersion);

            EXPECT_THROW(
                StateServer _server(
                    &_rpcServer,
                    {{"MachineFG", "Off"}, {"OtherFG", "Idle"}},
                    {{"MachineFG", "Off"}}),
                std::logic_error);
        }

        TEST_F(StateServerTest, InitialStateProperty)
        {
            const std::string cFunctionGroup{"MachineFG"};
            const std::string cExpectedState{"Off"};

            std::string _actualState;
            EXPECT_TRUE(Server.TryGetState(cFunctionGroup, _actualState));
            EXPECT_EQ(cExpectedState, _actualState);
        }

        TEST_F(StateServerTest, BackAndForthTransitionScenario)
        {
            const com::someip::SomeIpReturnCode cExpectedReturnCode{
                com::someip::SomeIpReturnCode::eOK};
            const std::string cFunctionGroup{"MachineFG"};
            const std::string cInitialState{"Off"};
            const std::string cNextState{"StartUp"};

            com::someip::rpc::SomeIpRpcMessage _response{
                Send(
                    cSetStateMessageId,
                    makeSetStatePayload(cFunctionGroup, cNextState))};
            EXPECT_EQ(cExpectedReturnCode, _response.ReturnCode());

            _response =
                Send(
                    cSetStateMessageId,
                    makeSetStatePayload(cFunctionGroup, cInitialState));
            EXPECT_EQ(cExpectedReturnCode, _response.ReturnCode());

            std::string _actualState;
            EXPECT_TRUE(Server.TryGetState(cFunctionGroup, _actualState));
            EXPECT_EQ(cInitialState, _actualState);
        }

        TEST_F(StateServerTest, UnknownFunctionGroupScenario)
        {
            const ExecErrc cExpectedResult{ExecErrc::kInvalidTransition};
            const std::string cFunctionGroup{"OtherFG"};
            const std::string cState{"Off"};

            com::someip::rpc::SomeIpRpcMessage _response{
                Send(
                    cSetStateMessageId,
                    makeSetStatePayload(cFunctionGroup, cState))};

            ExecErrc _actualResult;
            EXPECT_TRUE(
                helper::MockRpcServer::TryGetErrorCode(_response, _actualResult));
            EXPECT_EQ(cExpectedResult, _actualResult);

            std::string _actualState;
            EXPECT_FALSE(Server.TryGetState(cFunctionGroup, _actualState));
        }

        TEST_F(StateServerTest, OversizedFunctionGroupLengthScenario)
        {
            const ExecErrc cExpectedResult{ExecErrc::kInvalidArguments};

            std::vector<uint8_t> _rpcPayload({0xff, 0xff, 0xff, 0xff,
                                              77, 97, 99, 104, 105, 110, 101, 70, 71,
                                              0, 0, 0, 3,
                                              79, 102, 102});
            com::someip::rpc::SomeIpRpcMessage _response{
                Send(cSetStateMessageId, std::move(_rpcPayload))};

            ExecErrc _actualResult;
            EXPECT_TRUE(
                helper::MockRpcServer::TryGetErrorCode(_response, _actualResult));
            EXPECT_EQ(cExpectedResult, _actualResult);
        }

        TEST_F(StateServerTest, OversizedStateLengthScenario)
        {
            const ExecErrc cExpectedResult{ExecErrc::kInvalidArguments};

            std::vector<uint8_t> _rpcPayload({0, 0, 0, 9,
                                              77, 97, 99, 104, 105, 110, 101, 70, 71,
                                              0xff, 0xff, 0xff, 0xff,
                                              83, 116, 97, 114, 116, 85, 112});
            com::someip::rpc::SomeIpRpcMessage _response{
                Send(cSetStateMessageId, std::move(_rpcPayload))};

            ExecErrc _actualResult;
            EXPECT_TRUE(
                helper::MockRpcServer::TryGetErrorCode(_response, _actualResult));
            EXPECT_EQ(cExpectedResult, _actualResult);
        }

        TEST_F(StateServerTest, RejectedTransitionKeepsState)
        {
            const std::string cFunctionGroup{"MachineFG"};
            const std::string cExpectedState{"Off"};
            const std::string cInvalidState{"Diag"};

            Send(
                cSetStateMessageId,
                makeSetStatePayload(cFunctionGroup, cInvalidState));

            std::string _actualState;
            EXPECT_TRUE(Server.TryGetState(cFunctionGroup, _actualState));
            EXPECT_EQ(cExpectedState, _actualState);
        }

        TEST_F(StateServerTest, SetNotifierUnknownFunctionGroup)
        {
            const std::string cFunctionGroup{"OtherFG"};
            std::function<void()> _callback{[]() {}};

            EXPECT_THROW(
                Server.SetNotifier(cFunctionGroup, _callback), std::out_of_range);
        }

        TEST_F(StateServerTest, SetNotifierReplacement)
        {
            const std::string cFunctionGroup{"MachineFG"};
            const std::string cState{"StartUp"};
            const int cExpectedReplacedCount{0};
            const int cExpectedCount{1};

            int _replacedCount{0};
            int _count{0};
            std::function<void()> _replacedCallback{[&]()
                                                    { ++_replacedCount; }};
            std::function<void()> _callback{[&]()
                                            { ++_count; }};
            Server.SetNotifier(cFunctionGroup, _replacedCallback);
            Server.SetNotifier(cFunctionGroup, _callback);

            Send(cSetStateMessageId, makeSetStatePayload(cFunctionGroup, cState));

            EXPECT_EQ(cExpectedReplacedCount, _replacedCount);
            EXPECT_EQ(cExpectedCount, _count);
        }

        TEST_F(StateServerTest, NotifierOnRejectedTransition)
        {
            const std::string cFunctionGroup{"MachineFG"};
            const std::string cCurrentState{"Off"};
            const std::string cInvalidState{"Diag"};

            bool _notified{false};
            std::function<void()> _callback{[&]()
                                            { _notified = true; }};
            Server.SetNotifier(cFunctionGroup, _callback);

            Send(
                cSetStateMessageId,
                makeSetStatePayload(cFunctionGroup, cCurrentState));
            Send(
                cSetStateMessageId,
                makeSetStatePayload(cFunctionGroup, cInvalidState));

            EXPECT_FALSE(_notified);
        }

        TEST_F(StateServerTest, NotifierObservesNewState)
        {
            const std::string cFunctionGroup{"MachineFG"};
            const std::string cExpectedState{"StartUp"};

            std::string _observedState;
            std::function<void()> _callback{
                [&]()
                {
                    // The server lock should be released while notifying
                    Server.TryGetState(cFunctionGroup, _observedState);
                }};
            Server.SetNotifier(cFunctionGroup, _callback);

            Send(
                cSetStateMessageId,
                makeSetStatePayload(cFunctionGroup, cExpectedState));

            EXPECT_EQ(cExpectedState, _observedState);
        }
    }
}