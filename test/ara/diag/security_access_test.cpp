#include "./routing/testable_uds_service.h"
#include "../../../src/ara/diag/security_access.h"

namespace ara
{
    namespace diag
    {
        class SecurityAccessTest : public routing::TestableUdsService
        {
        private:
            static core::InstanceSpecifier mSpecifier;
            static const ReentrancyType cReentrancy;

            const uint8_t cAttemptThreshold{1};
            const int cExceededAttemptDelay{3600};

            const std::string cEncryptorKey{"Encryptor"};
            const std::string cAttemptThresholdKey{"AttemptThreshold"};
            const std::string cExceededAttemptDelayKey{"ExceededAttemptDelay"};

        protected:
            const uint16_t cEncryptor{0x1234};

            SecurityAccess Service;

        public:
            SecurityAccessTest() : Service{mSpecifier, cReentrancy}
            {
                std::string _encryptorStr{std::to_string(cEncryptor)};
                GeneralMetaInfo.SetValue(cEncryptorKey, _encryptorStr);

                std::string _attemptThresholdStr{std::to_string(cAttemptThreshold)};
                GeneralMetaInfo.SetValue(cAttemptThresholdKey, _attemptThresholdStr);

                std::string _exceededAttemptDelayStr{std::to_string(cExceededAttemptDelay)};
                GeneralMetaInfo.SetValue(cExceededAttemptDelayKey, _exceededAttemptDelayStr);
            }

            uint16_t GetSeed(uint8_t subFunction)
            {
                const size_t cSeedOffset{0};

                std::vector<uint8_t> _securityAccessDataRecord;
                CancellationHandler _cancellationHandler(false);

                std::future<std::vector<uint8_t>> _resultByteArrFuture{
                    Service.GetSeed(
                        subFunction,
                        _securityAccessDataRecord,
                        GeneralMetaInfo,
                        std::move(_cancellationHandler))};

                std::vector<uint8_t> _resultByteArr{_resultByteArrFuture.get()};
                size_t _offset{cSeedOffset};
                auto _result{static_cast<uint16_t>(_resultByteArr.at(_offset)) << 8};
                ++_offset;
                _result |= _resultByteArr.at(_offset);

                return _result;
            }
        };

        core::InstanceSpecifier SecurityAccessTest::mSpecifier{"Instance0"};
        const ReentrancyType SecurityAccessTest::cReentrancy{ReentrancyType::kNot};

        TEST_F(SecurityAccessTest, IncorrectRequestScenario)
        {
            const uint8_t cExpectedNrc{0x13};

            uint8_t _actualNrc;
            std::vector<uint8_t> _requestData{Service.GetSid()};
            bool _hasNrc{TryGetNrc(&Service, _requestData, _actualNrc)};

            EXPECT_TRUE(_hasNrc);
            EXPECT_EQ(cExpectedNrc, _actualNrc);
        }

        TEST_F(SecurityAccessTest, InvalidSubFunctionScenario)
        {
            const uint8_t cExpectedNrc{0x12};
            const uint8_t cSubFunction{0x43};

            uint8_t _actualNrc;
            std::vector<uint8_t> _requestData{Service.GetSid(), cSubFunction};
            bool _hasNrc{TryGetNrc(&Service, _requestData, _actualNrc)};

            EXPECT_TRUE(_hasNrc);
            EXPECT_EQ(cExpectedNrc, _actualNrc);
        }

        TEST_F(SecurityAccessTest, GetSeedMethod)
        {
            const uint8_t cSubFunction{0x01};

            uint16_t _expectedResult{GetSeed(cSubFunction)};
            EXPECT_GT(_expectedResult, 0);

            uint16_t _actualResult{GetSeed(cSubFunction)};
            EXPECT_EQ(_expectedResult, _actualResult);
        }

        TEST_F(SecurityAccessTest, IncorrectKeyLengthScenario)
        {
            const uint8_t cSubFunction{0x02};
            const uint8_t cExpectedNrc{0x13};

            uint8_t _actualNrc;
            std::vector<uint8_t> _requestData{Service.GetSid(), cSubFunction};
            bool _hasNrc{TryGetNrc(&Service, _requestData, _actualNrc)};

            EXPECT_TRUE(_hasNrc);
            EXPECT_EQ(cExpectedNrc, _actualNrc);
        }

        TEST_F(SecurityAccessTest, InvalidCompareKeyMethod)
        {
            const uint8_t cSubFunction{0x01};
            const uint8_t cKeyMsb{0x00};
            const uint8_t cKeyLsb{0x00};
            const KeyCompareResultType cExpectedResult{KeyCompareResultType::kKeyInvalid};

            GetSeed(cSubFunction);

            std::vector<uint8_t> _keyByteArr{cKeyMsb, cKeyMsb};
            CancellationHandler _cancellationHandler(false);

            std::future<KeyCompareResultType> _actualResultFuture{
                Service.CompareKey(
                    cSubFunction,
                    _keyByteArr,
                    GeneralMetaInfo,
                    std::move(_cancellationHandler))};

            KeyCompareResultType _actualResult{_actualResultFuture.get()};

            EXPECT_EQ(cExpectedResult, _actualResult);
        }

        TEST_F(SecurityAccessTest, ValidCompareKeyMethod)
        {
            const uint8_t cSubFunction{0x01};
            const KeyCompareResultType cExpectedResult{KeyCompareResultType::kKeyValid};

            uint16_t _seed{GetSeed(cSubFunction)};

            auto _key{static_cast<uint16_t>(_seed ^ cEncryptor)};
            auto _keyMsb{static_cast<uint8_t>(_key >> 8)};
            auto _keyLsb{static_cast<uint8_t>(_key)};
            std::vector<uint8_t> _keyByteArr{_keyMsb, _keyLsb};

            CancellationHandler _cancellationHandler(false);

            std::future<KeyCompareResultType> _actualResultFuture{
                Service.CompareKey(
                    cSubFunction,
                    _keyByteArr,
                    GeneralMetaInfo,
                    std::move(_cancellationHandler))};

            KeyCompareResultType _actualResult{_actualResultFuture.get()};

            EXPECT_EQ(cExpectedResult, _actualResult);
        }

        TEST_F(SecurityAccessTest, SequenceErrorScenario)
        {
            const uint8_t cExpectedNrc{0x24};
            const uint8_t cSubFunction{0x02};
            const uint8_t cKeyMsb{0x00};
            const uint8_t cKeyLsb{0x00};

            std::vector<uint8_t> _requestData{Service.GetSid(), cSubFunction, cKeyMsb, cKeyLsb};
            uint8_t _actualNrc;
            bool _hasNrc{TryGetNrc(&Service, _requestData, _actualNrc)};

            EXPECT_TRUE(_hasNrc);
            EXPECT_EQ(cExpectedNrc, _actualNrc);
        }

        TEST_F(SecurityAccessTest, InvalidKeyScenrio)
        {
            const uint8_t cExpectedNrc{0x35};
            const uint8_t cSubFunction{0x01};
            const uint8_t cKeyMsb{0x00};
            const uint8_t cKeyLsb{0x00};

            GetSeed(cSubFunction);

            auto _keyCompareSubFunction{static_cast<uint8_t>(cSubFunction + 1)};
            std::vector<uint8_t> _requestData{Service.GetSid(), _keyCompareSubFunction, cKeyMsb, cKeyLsb};
            uint8_t _actualNrc;
            bool _hasNrc{TryGetNrc(&Service, _requestData, _actualNrc)};

            EXPECT_TRUE(_hasNrc);
            EXPECT_EQ(cExpectedNrc, _actualNrc);
        }

        TEST_F(SecurityAccessTest, ExceededNumberOfAttemptsScenario)
        {
            const uint8_t cExpectedNrc{0x36};
            const uint8_t cSubFunction{0x01};
            const uint8_t cKeyMsb{0x00};
            const uint8_t cKeyLsb{0x00};

            GetSeed(cSubFunction);

            auto _keyCompareSubFunction{static_cast<uint8_t>(cSubFunction + 1)};
            std::vector<uint8_t> _requestData{Service.GetSid(), _keyCompareSubFunction, cKeyMsb, cKeyLsb};
            uint8_t _actualNrc;
            TryGetNrc(&Service, _requestData, _actualNrc);
            bool _hasNrc{TryGetNrc(&Service, _requestData, _actualNrc)};

            EXPECT_TRUE(_hasNrc);
            EXPECT_EQ(cExpectedNrc, _actualNrc);
        }

        TEST_F(SecurityAccessTest, ValidKeyScenrio)
        {
            const uint8_t cSubFunction{0x01};
            const size_t cSidIndex{0};
            const size_t cSubFunctionIndex{1};
            const uint8_t cPositiveResponseSidIncrement{0x40};

            uint16_t _seed{GetSeed(cSubFunction)};
            auto _key{static_cast<uint16_t>(_seed ^ cEncryptor)};
            auto _keyMsb{static_cast<uint8_t>(_key >> 8)};
            auto _keyLsb{static_cast<uint8_t>(_key)};

            auto _keyCompareSubFunction{static_cast<uint8_t>(cSubFunction + 1)};
            std::vector<uint8_t> _requestData{Service.GetSid(), _keyCompareSubFunction, _keyMsb, _keyLsb};
            CancellationHandler _cancellationHandler(false);

            std::future<OperationOutput> _responseFuture{
                Service.HandleMessage(
                    _requestData, GeneralMetaInfo, std::move(_cancellationHandler))};

            OperationOutput _response{_responseFuture.get()};

            auto _expectedSid{static_cast<uint8_t>(Service.GetSid() + cPositiveResponseSidIncrement)};
            uint8_t _actualSid{_response.responseData.at(cSidIndex)};
            EXPECT_EQ(_expectedSid, _actualSid);

            uint8_t _subFunction{_response.responseData.at(cSubFunctionIndex)};
            EXPECT_EQ(_keyCompareSubFunction, _subFunction);
        }

        TEST_F(SecurityAccessTest, UnlockedLevelSeedScenario)
        {
            const uint8_t cSubFunction{0x01};
            const uint8_t cExpectedResult{0};

            uint16_t _seed{GetSeed(cSubFunction)};
            auto _key{static_cast<uint16_t>(_seed ^ cEncryptor)};
            auto _keyMsb{static_cast<uint8_t>(_key >> 8)};
            auto _keyLsb{static_cast<uint8_t>(_key)};

            auto _keyCompareSubFunction{static_cast<uint8_t>(cSubFunction + 1)};
            std::vector<uint8_t> _requestData{Service.GetSid(), _keyCompareSubFunction, _keyMsb, _keyLsb};
            CancellationHandler _cancellationHandler(false);

            Service.HandleMessage(_requestData, GeneralMetaInfo, std::move(_cancellationHandler));

            uint16_t _actualResult{GetSeed(cSubFunction)};

            EXPECT_EQ(cExpectedResult, _actualResult);
        }

        class SecurityAccessMessageTest : public SecurityAccessTest
        {
        protected:
            const uint8_t cPositiveResponseSid{0x67};

            std::vector<uint8_t> Send(const std::vector<uint8_t> &requestData)
            {
                CancellationHandler _cancellationHandler(false);
                std::future<OperationOutput> _responseFuture{
                    Service.HandleMessage(
                        requestData, GeneralMetaInfo, std::move(_cancellationHandler))};
                OperationOutput _response{_responseFuture.get()};

                return _response.responseData;
            }

            std::vector<uint8_t> SendValidKey(
                uint8_t requestSeedSubFunction, uint8_t sendKeySubFunction)
            {
                uint16_t _seed{GetSeed(requestSeedSubFunction)};
                uint16_t _key{static_cast<uint16_t>(_seed ^ cEncryptor)};
                uint8_t _keyMsb{static_cast<uint8_t>(_key >> 8)};
                uint8_t _keyLsb{static_cast<uint8_t>(_key)};
                std::vector<uint8_t> _requestData{
                    Service.GetSid(), sendKeySubFunction, _keyMsb, _keyLsb};

                return Send(_requestData);
            }
        };

        TEST_F(SecurityAccessMessageTest, RequestSeedScenario)
        {
            const uint8_t cSubFunction{0x01};
            const size_t cExpectedSize{3};
            const uint8_t cExpectedSeedMsb{0x00};
            const uint8_t cExpectedSeedLsb{0x01};
            const uint16_t cExpectedSeed{0x0001};

            std::vector<uint8_t> _requestData{Service.GetSid(), cSubFunction};
            std::vector<uint8_t> _response{Send(_requestData)};

            ASSERT_EQ(cExpectedSize, _response.size());
            EXPECT_EQ(cExpectedSeedMsb, _response.at(1));
            EXPECT_EQ(cExpectedSeedLsb, _response.at(2));

            // The same seed is returned as long as the level is locked
            EXPECT_EQ(cExpectedSeed, GetSeed(cSubFunction));
        }

        // BUG (AE-30): SecurityAccess::handleRequestSeed prefixes the seed with the request SID (0x27)
        // and does not echo the sub-function, while ISO 14229-1 expects
        // the positive response SID (0x67) followed by the sub-function and the seed.
        TEST_F(SecurityAccessMessageTest, DISABLED_BugRequestSeedPositiveResponseFormat)
        {
            const uint8_t cSubFunction{0x01};
            const std::vector<uint8_t> cExpectedResponse{
                cPositiveResponseSid, cSubFunction, 0x00, 0x01};

            std::vector<uint8_t> _requestData{Service.GetSid(), cSubFunction};
            std::vector<uint8_t> _response{Send(_requestData)};

            EXPECT_EQ(cExpectedResponse, _response);
        }

        TEST_F(SecurityAccessMessageTest, SuppressedRequestSeedScenario)
        {
            const uint8_t cSubFunction{0x81};

            std::vector<uint8_t> _requestData{Service.GetSid(), cSubFunction};
            std::vector<uint8_t> _response{Send(_requestData)};

            EXPECT_TRUE(_response.empty());
        }

        TEST_F(SecurityAccessMessageTest, SeedsOfDifferentLevels)
        {
            const uint8_t cFirstSubFunction{0x01};
            const uint8_t cSecondSubFunction{0x03};
            const uint16_t cExpectedFirstSeed{1};
            const uint16_t cExpectedSecondSeed{2};

            EXPECT_EQ(cExpectedFirstSeed, GetSeed(cFirstSubFunction));
            EXPECT_EQ(cExpectedSecondSeed, GetSeed(cSecondSubFunction));
            EXPECT_EQ(cExpectedFirstSeed, GetSeed(cFirstSubFunction));
        }

        TEST_F(SecurityAccessMessageTest, SuppressedValidKeyScenario)
        {
            const uint8_t cRequestSeedSubFunction{0x81};
            const uint8_t cSendKeySubFunction{0x82};
            const uint16_t cUnlockedSeed{0};

            std::vector<uint8_t> _response{
                SendValidKey(cRequestSeedSubFunction, cSendKeySubFunction)};

            EXPECT_TRUE(_response.empty());
            // Zero seed shows that the level has been unlocked.
            EXPECT_EQ(cUnlockedSeed, GetSeed(cRequestSeedSubFunction));
        }

        // BUG (AE-30): The suppressPosRspMsgIndicationBit is not masked out of the security level,
        // so a suppressed send key (0x82) does not match the seed requested without suppression (0x01)
        // and it is rejected by the request sequence error NRC (0x24).
        TEST_F(SecurityAccessMessageTest, DISABLED_BugSuppressedSendKeyAfterUnsuppressedRequestSeed)
        {
            const uint8_t cRequestSeedSubFunction{0x01};
            const uint8_t cSendKeySubFunction{0x82};
            const uint16_t cUnlockedSeed{0};

            std::vector<uint8_t> _response{
                SendValidKey(cRequestSeedSubFunction, cSendKeySubFunction)};

            EXPECT_TRUE(_response.empty());
            EXPECT_EQ(cUnlockedSeed, GetSeed(cRequestSeedSubFunction));
        }

        // BUG (AE-30): SecurityAccess::handlePassedAttempt iterates the security levels by value,
        // so the previously unlocked levels are never locked again
        // while the code comment (and ISO 14229-1) expects only one unlocked level at a time.
        TEST_F(SecurityAccessMessageTest, DISABLED_BugUnlockingAnotherLevelLocksPreviousLevel)
        {
            const uint8_t cFirstRequestSeedSubFunction{0x01};
            const uint8_t cFirstSendKeySubFunction{0x02};
            const uint8_t cSecondRequestSeedSubFunction{0x03};
            const uint8_t cSecondSendKeySubFunction{0x04};
            const uint16_t cUnlockedSeed{0};

            SendValidKey(cFirstRequestSeedSubFunction, cFirstSendKeySubFunction);
            EXPECT_EQ(cUnlockedSeed, GetSeed(cFirstRequestSeedSubFunction));

            SendValidKey(cSecondRequestSeedSubFunction, cSecondSendKeySubFunction);
            EXPECT_EQ(cUnlockedSeed, GetSeed(cSecondRequestSeedSubFunction));

            // The first level should be locked again, i.e., its seed should not be zero.
            EXPECT_NE(cUnlockedSeed, GetSeed(cFirstRequestSeedSubFunction));
        }

        TEST_F(SecurityAccessMessageTest, RequestSeedWhileDelayTimerIsActive)
        {
            const uint8_t cExpectedNrc{0x36};
            const uint8_t cRequestSeedSubFunction{0x01};
            const uint8_t cSendKeySubFunction{0x02};
            const uint8_t cKeyMsb{0x00};
            const uint8_t cKeyLsb{0x00};

            GetSeed(cRequestSeedSubFunction);

            // Exceed the attempt threshold to activate the delay timer
            std::vector<uint8_t> _invalidKeyRequestData{
                Service.GetSid(), cSendKeySubFunction, cKeyMsb, cKeyLsb};
            Send(_invalidKeyRequestData);
            Send(_invalidKeyRequestData);

            uint8_t _actualNrc;
            std::vector<uint8_t> _requestData{Service.GetSid(), cRequestSeedSubFunction};
            bool _hasNrc{TryGetNrc(&Service, _requestData, _actualNrc)};

            EXPECT_TRUE(_hasNrc);
            EXPECT_EQ(cExpectedNrc, _actualNrc);
        }

        TEST_F(SecurityAccessMessageTest, CanceledInvalidRequestScenario)
        {
            const std::vector<uint8_t> cExpectedResponse{0x7f, 0x27, 0x10};

            std::vector<uint8_t> _requestData{Service.GetSid()};
            CancellationHandler _cancellationHandler(true);

            std::future<OperationOutput> _responseFuture{
                Service.HandleMessage(
                    _requestData, GeneralMetaInfo, std::move(_cancellationHandler))};
            OperationOutput _response{_responseFuture.get()};

            EXPECT_EQ(cExpectedResponse, _response.responseData);
        }

        TEST_F(SecurityAccessMessageTest, CompareKeyWithoutEncryptor)
        {
            const uint8_t cSubFunction{0x01};
            const KeyCompareResultType cExpectedResult{KeyCompareResultType::kKeyInvalid};

            GetSeed(cSubFunction);

            MetaInfo _metaInfo(Context::kDoIP);
            std::vector<uint8_t> _keyByteArr{0x12, 0x35};
            CancellationHandler _cancellationHandler(false);

            std::future<KeyCompareResultType> _actualResultFuture{
                Service.CompareKey(
                    cSubFunction,
                    _keyByteArr,
                    _metaInfo,
                    std::move(_cancellationHandler))};
            KeyCompareResultType _actualResult{_actualResultFuture.get()};

            EXPECT_EQ(cExpectedResult, _actualResult);
        }
    }
}