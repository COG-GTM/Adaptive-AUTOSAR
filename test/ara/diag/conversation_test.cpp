#include <gtest/gtest.h>
#include "../../../src/ara/diag/conversation.h"

namespace ara
{
    namespace diag
    {
        class ConversationTest : public testing::Test
        {
        protected:
            MetaInfo DoipMetaInfo;
            std::reference_wrapper<Conversation> ConversationReference;
            SessionControlType ActiveSession;

        public:
            ConversationTest() : DoipMetaInfo(Context::kDoIP),
                                 ConversationReference{Conversation::GetConversation(DoipMetaInfo).Value()},
                                 ActiveSession{SessionControlType::kDefaultSession}
            {
            }

            void ActiveSessionChanged(SessionControlType activeSession)
            {
                ActiveSession = activeSession;
            }
        };

        TEST_F(ConversationTest, Factory)
        {
            const ActivityStatusType cExpectedActivityStatus{
                ActivityStatusType::kActive};
            ActivityStatusType _actualActivityStatus{
                ConversationReference.get().GetActivityStatus().Value()};
            EXPECT_EQ(cExpectedActivityStatus, _actualActivityStatus);

            const SessionControlType cExpectedDiagnosticSession{
                SessionControlType::kDefaultSession};
            SessionControlType _actualDiagnosticSession{
                ConversationReference.get().GetDiagnosticSession().Value()};
            EXPECT_EQ(cExpectedDiagnosticSession, _actualDiagnosticSession);

            const SecurityLevelType cExpectedSecuirtyLevel{
                SecurityLevelType::kLocked};
            SecurityLevelType _actualSecurityLevel{
                ConversationReference.get().GetDiagnosticSecurityLevel().Value()};
            EXPECT_EQ(cExpectedSecuirtyLevel, _actualSecurityLevel);
        }

        TEST_F(ConversationTest, GetAllConversationsMethod)
        {
            auto _converstation{Conversation::GetAllConversations()};

            const size_t cExpectedResult{1};
            size_t _actualResult{_converstation.size()};

            EXPECT_EQ(cExpectedResult, _actualResult);
        }

        TEST_F(ConversationTest, GetCurrentActiveConversationsMethod)
        {
            auto _activeConverstations{Conversation::GetCurrentActiveConversations()};

            const size_t cExpectedResult{1};
            size_t _actualResult{_activeConverstations.size()};

            EXPECT_EQ(cExpectedResult, _actualResult);
        }

        TEST_F(ConversationTest, DiagnosticSessionScenario)
        {
            const SessionControlType cDefaultSession{
                SessionControlType::kDefaultSession};
            const SessionControlType cNewSession{
                SessionControlType::kProgrammingSession};

            auto _callback{
                std::bind(
                    &ConversationTest::ActiveSessionChanged,
                    this, std::placeholders::_1)};

            ConversationReference.get().SetDiagnosticSessionNotifier(_callback);
            ConversationReference.get().SetDiagnosticSession(cNewSession);
            EXPECT_EQ(cNewSession, ActiveSession);

            ConversationReference.get().ResetToDefaultSession();
            EXPECT_EQ(cDefaultSession, ActiveSession);
        }

        TEST_F(ConversationTest, DeactivateMethod)
        {
            const ActivityStatusType cExpectedResult{
                ActivityStatusType::kInactive};

            ActivityStatusType _actualResult;

            auto _callback = [&](ActivityStatusType status)
            {
                _actualResult = status;
            };

            ConversationReference.get().SetActivityNotifier(_callback);

            ConversationReference.get().Deactivate();
            EXPECT_EQ(cExpectedResult, _actualResult);

            _actualResult = ConversationReference.get().GetActivityStatus().Value();
            EXPECT_EQ(cExpectedResult, _actualResult);

            auto _activeConverstations{Conversation::GetCurrentActiveConversations()};
            EXPECT_TRUE(_activeConverstations.empty());
        }

        TEST_F(ConversationTest, ConversationIdentifierProperty)
        {
            const int64_t cExpectedResult{-1};

            ara::core::Result<ConversationIdentifierType> _identifier{
                ConversationReference.get().GetConversationIdentifier()};

            EXPECT_TRUE(_identifier.HasValue());
            EXPECT_EQ(cExpectedResult, _identifier.Value().id);
        }

        TEST_F(ConversationTest, DiagnosticSessionShortNameMethod)
        {
            const SessionControlType cUnsupportedSession{
                static_cast<SessionControlType>(0x7f)};

            Conversation &_conversation{ConversationReference.get()};

            EXPECT_EQ(
                "Default session",
                _conversation.GetDiagnosticSessionShortName(
                    SessionControlType::kDefaultSession).Value());
            EXPECT_EQ(
                "Programming session",
                _conversation.GetDiagnosticSessionShortName(
                    SessionControlType::kProgrammingSession).Value());
            EXPECT_EQ(
                "Extended diagnostic session",
                _conversation.GetDiagnosticSessionShortName(
                    SessionControlType::kExtendedDiagnosticSession).Value());
            EXPECT_EQ(
                "Safety system diagnostic session",
                _conversation.GetDiagnosticSessionShortName(
                    SessionControlType::kSafetySystemDiagnosticSession).Value());
            EXPECT_EQ(
                "Not supported",
                _conversation.GetDiagnosticSessionShortName(
                    cUnsupportedSession).Value());
        }

        TEST_F(ConversationTest, DiagnosticSecurityLevelShortNameMethod)
        {
            const SecurityLevelType cUnsupportedLevel{
                static_cast<SecurityLevelType>(0x7f)};

            Conversation &_conversation{ConversationReference.get()};

            EXPECT_EQ(
                "Security level locked",
                _conversation.GetDiagnosticSecurityLevelShortName(
                    SecurityLevelType::kLocked).Value());
            EXPECT_EQ(
                "Not supported",
                _conversation.GetDiagnosticSecurityLevelShortName(
                    cUnsupportedLevel).Value());
        }

        TEST_F(ConversationTest, ResetToDefaultSessionMethod)
        {
            const uint32_t cExpectedSecurityLevelCallCount{0};

            Conversation &_conversation{ConversationReference.get()};
            uint32_t _securityLevelCallCount{0};
            std::function<void(SecurityLevelType)> _securityLevelNotifier{
                [&_securityLevelCallCount](SecurityLevelType)
                {
                    ++_securityLevelCallCount;
                }};

            ara::core::Result<void> _notifierResult{
                _conversation.SetSecurityLevelNotifier(_securityLevelNotifier)};
            EXPECT_TRUE(_notifierResult.HasValue());

            _conversation.SetDiagnosticSessionNotifier(
                std::bind(
                    &ConversationTest::ActiveSessionChanged,
                    this, std::placeholders::_1));
            _conversation.SetDiagnosticSession(
                SessionControlType::kExtendedDiagnosticSession);
            EXPECT_EQ(SessionControlType::kExtendedDiagnosticSession, ActiveSession);

            ara::core::Result<void> _resetResult{
                _conversation.ResetToDefaultSession()};
            EXPECT_TRUE(_resetResult.HasValue());
            EXPECT_EQ(SessionControlType::kDefaultSession, ActiveSession);
            EXPECT_EQ(
                SessionControlType::kDefaultSession,
                _conversation.GetDiagnosticSession().Value());
            EXPECT_EQ(
                SecurityLevelType::kLocked,
                _conversation.GetDiagnosticSecurityLevel().Value());
            // The security level is already locked, so no notification is expected.
            EXPECT_EQ(cExpectedSecurityLevelCallCount, _securityLevelCallCount);
        }

        TEST_F(ConversationTest, DeactivateNotifiesOnlyOnce)
        {
            const uint32_t cExpectedCallCount{1};

            Conversation &_conversation{ConversationReference.get()};
            uint32_t _callCount{0};
            std::function<void(ActivityStatusType)> _notifier{
                [&_callCount](ActivityStatusType)
                {
                    ++_callCount;
                }};

            _conversation.SetActivityNotifier(_notifier);
            _conversation.Deactivate();
            _conversation.Deactivate();

            EXPECT_EQ(cExpectedCallCount, _callCount);
        }

        // BUG (AE-30): Conversation::GetCurrentActiveConversations iterates the
        // conversations by value and returns references to the loop copy, so
        // the returned references dangle instead of referring to the stored conversations.
        // The test only compares addresses, i.e., it does not use the dangling references.
        TEST_F(ConversationTest, DISABLED_BugActiveConversationsReferToStoredConversations)
        {
            std::vector<std::reference_wrapper<Conversation>> _allConversations(
                Conversation::GetAllConversations());
            std::vector<std::reference_wrapper<Conversation>> _activeConversations(
                Conversation::GetCurrentActiveConversations());

            ASSERT_FALSE(_activeConversations.empty());

            for (const std::reference_wrapper<Conversation> &_activeConversation : _activeConversations)
            {
                bool _found{false};
                for (const std::reference_wrapper<Conversation> &_conversation : _allConversations)
                {
                    if (&_conversation.get() == &_activeConversation.get())
                    {
                        _found = true;
                    }
                }

                EXPECT_TRUE(_found);
            }
        }
    }
}