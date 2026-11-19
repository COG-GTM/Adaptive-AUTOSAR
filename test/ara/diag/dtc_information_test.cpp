#include <gtest/gtest.h>
#include "../../../src/ara/diag/diag_error_domain.h"
#include "../../../src/ara/diag/dtc_information.h"

namespace ara
{
    namespace diag
    {
        class DtcInformationTest : public testing::Test
        {
        protected:
            const ControlDtcStatusType cInitialControlDtcStatus{
                ControlDtcStatusType::kDTCSettingOff};

            ara::core::InstanceSpecifier Specifier;
            ControlDtcStatusType NotifiedControlDtcStatus;
            uint8_t LastChangedDtc;
            UdsDtcStatusByteType LastChangedDtcOldStatusByte;
            UdsDtcStatusByteType LastChangedDtcNewStatusByte;
            uint32_t NotifiedNumberOfStoredEntries;

        public:
            DtcInformationTest() : Specifier{"Instance0"},
                                   NotifiedControlDtcStatus{cInitialControlDtcStatus},
                                   LastChangedDtc{0},
                                   NotifiedNumberOfStoredEntries{0}
            {
                LastChangedDtcOldStatusByte.encodedBits = 0;
                LastChangedDtcNewStatusByte.encodedBits = 0;
            }

            void OnControlDtcStatusChanged(ControlDtcStatusType controlDtcStatus)
            {
                NotifiedControlDtcStatus = controlDtcStatus;
            }

            void OnDtcStatusChanged(
                uint32_t dtc,
                UdsDtcStatusByteType oldStatusByte,
                UdsDtcStatusByteType newStatusByte)
            {
                LastChangedDtc = dtc;
                LastChangedDtcOldStatusByte = oldStatusByte;
                LastChangedDtcNewStatusByte = newStatusByte;
            }

            void OnNumberOfStoredEntriesChanged(uint32_t numberOfStoredEntries)
            {
                NotifiedNumberOfStoredEntries = numberOfStoredEntries;
            };
        };

        TEST_F(DtcInformationTest, Constructor)
        {
            uint32_t cExpectedResult{0};

            DTCInformation _dtcInformation(Specifier);
            auto _actualResult{_dtcInformation.GetNumberOfStoredEntries()};

            EXPECT_TRUE(_actualResult.HasValue());
            EXPECT_EQ(cExpectedResult, _actualResult.Value());
        };

        TEST_F(DtcInformationTest, CurrentStatusProperty)
        {
            const uint32_t cDtc{1};
            const UdsDtcStatusBitType cMask{UdsDtcStatusBitType::kTestFailed};
            const uint8_t cStatus{0x01};

            UdsDtcStatusByteType _expectedResult;
            _expectedResult.encodedBits = cStatus;

            DTCInformation _dtcInformation(Specifier);
            _dtcInformation.SetCurrentStatus(cDtc, cMask, _expectedResult);
            auto _actualResult{_dtcInformation.GetCurrentStatus(cDtc)};

            EXPECT_TRUE(_actualResult.HasValue());
            EXPECT_EQ(_expectedResult.encodedBits, _actualResult.Value().encodedBits);
        };

        TEST_F(DtcInformationTest, DtcStatusChangedNotifier)
        {
            const uint32_t cDtc{1};
            const UdsDtcStatusBitType cMask{UdsDtcStatusBitType::kTestFailed};
            const uint8_t cOldStatus{0x00};
            const uint8_t cNewStatus{0x01};

            UdsDtcStatusByteType _statusByte;
            _statusByte.encodedBits = cOldStatus;

            DTCInformation _dtcInformation(Specifier);
            _dtcInformation.SetCurrentStatus(cDtc, cMask, _statusByte);
            _dtcInformation.SetDTCStatusChangedNotifier(
                std::bind(
                    &DtcInformationTest::OnDtcStatusChanged,
                    this,
                    std::placeholders::_1,
                    std::placeholders::_2,
                    std::placeholders::_3));

            _statusByte.encodedBits = cNewStatus;
            _dtcInformation.SetCurrentStatus(cDtc, cMask, _statusByte);

            EXPECT_EQ(cDtc, LastChangedDtc);
            EXPECT_EQ(cOldStatus, LastChangedDtcOldStatusByte.encodedBits);
            EXPECT_EQ(cNewStatus, LastChangedDtcNewStatusByte.encodedBits);
        };

        TEST_F(DtcInformationTest, NumberOfStoredEntriesProperty)
        {
            const uint32_t cExpectedResult{1};
            const uint32_t cDtc{1};
            const UdsDtcStatusBitType cMask{UdsDtcStatusBitType::kTestFailed};
            const uint8_t cStatus{0x01};

            UdsDtcStatusByteType _statusByte;
            _statusByte.encodedBits = cStatus;

            DTCInformation _dtcInformation(Specifier);
            _dtcInformation.SetCurrentStatus(cDtc, cMask, _statusByte);
            auto _actualResult{_dtcInformation.GetNumberOfStoredEntries()};

            EXPECT_TRUE(_actualResult.HasValue());
            EXPECT_EQ(cExpectedResult, _actualResult.Value());
        };

        TEST_F(DtcInformationTest, NumberOfStoredEntriesNotifier)
        {
            const uint32_t cExpectedResult{1};
            const uint32_t cDtc{1};
            const UdsDtcStatusBitType cMask{UdsDtcStatusBitType::kTestFailed};
            const uint8_t cStatus{0x01};

            UdsDtcStatusByteType _statusByte;
            _statusByte.encodedBits = cStatus;

            DTCInformation _dtcInformation(Specifier);
            _dtcInformation.SetNumberOfStoredEntriesNotifier(
                std::bind(
                    &DtcInformationTest::OnNumberOfStoredEntriesChanged,
                    this, std::placeholders::_1));

            _dtcInformation.SetCurrentStatus(cDtc, cMask, _statusByte);

            EXPECT_EQ(cExpectedResult, NotifiedNumberOfStoredEntries);
        };

        TEST_F(DtcInformationTest, ClearMethod)
        {
            const uint32_t cExpectedResult{0};
            const uint32_t cDtc{1};
            const UdsDtcStatusBitType cMask{UdsDtcStatusBitType::kTestFailed};
            const uint8_t cStatus{0x01};

            UdsDtcStatusByteType _statusByte;
            _statusByte.encodedBits = cStatus;

            DTCInformation _dtcInformation(Specifier);
            _dtcInformation.SetCurrentStatus(cDtc, cMask, _statusByte);
            _dtcInformation.Clear(cDtc);
            auto _actualResult{_dtcInformation.GetNumberOfStoredEntries()};

            EXPECT_TRUE(_actualResult.HasValue());
            EXPECT_EQ(cExpectedResult, _actualResult.Value());
        };

        TEST_F(DtcInformationTest, ControlDtcStatusProperty)
        {
            ControlDtcStatusType cExpectedResult{ControlDtcStatusType::kDTCSettingOn};

            DTCInformation _dtcInformation(Specifier);
            _dtcInformation.EnableControlDtc();
            auto _actualResult{_dtcInformation.GetControlDTCStatus()};

            EXPECT_TRUE(_actualResult.HasValue());
            EXPECT_EQ(cExpectedResult, _actualResult.Value());
        };

        TEST_F(DtcInformationTest, ControlDtcStatusNotifier)
        {
            ControlDtcStatusType cExpectedResult{ControlDtcStatusType::kDTCSettingOn};

            DTCInformation _dtcInformation(Specifier);

            _dtcInformation.SetControlDtcStatusNotifier(
                std::bind(
                    &DtcInformationTest::OnControlDtcStatusChanged,
                    this, std::placeholders::_1));

            _dtcInformation.EnableControlDtc();

            EXPECT_EQ(cExpectedResult, NotifiedControlDtcStatus);
        };

        TEST_F(DtcInformationTest, CurrentStatusOfUnknownDtc)
        {
            const uint32_t cDtc{1};
            const DiagErrc cExpectedError{DiagErrc::kNoSuchDTC};

            DTCInformation _dtcInformation(Specifier);
            core::Result<UdsDtcStatusByteType> _result{
                _dtcInformation.GetCurrentStatus(cDtc)};

            ASSERT_FALSE(_result.HasValue());
            DiagErrc _actualError{static_cast<DiagErrc>(_result.Error().Value())};
            EXPECT_EQ(cExpectedError, _actualError);
        }

        TEST_F(DtcInformationTest, ClearUnknownDtc)
        {
            const uint32_t cDtc{1};
            const DiagErrc cExpectedError{DiagErrc::kWrongDtc};

            DTCInformation _dtcInformation(Specifier);
            core::Result<void> _result{_dtcInformation.Clear(cDtc)};

            ASSERT_FALSE(_result.HasValue());
            DiagErrc _actualError{static_cast<DiagErrc>(_result.Error().Value())};
            EXPECT_EQ(cExpectedError, _actualError);
        }

        TEST_F(DtcInformationTest, ClearNotifiesNumberOfStoredEntries)
        {
            const uint32_t cExpectedAddedResult{1};
            const uint32_t cExpectedClearedResult{0};
            const uint32_t cDtc{1};
            const UdsDtcStatusBitType cMask{UdsDtcStatusBitType::kTestFailed};

            UdsDtcStatusByteType _statusByte;
            _statusByte.encodedBits = 0x01;

            DTCInformation _dtcInformation(Specifier);
            _dtcInformation.SetNumberOfStoredEntriesNotifier(
                std::bind(
                    &DtcInformationTest::OnNumberOfStoredEntriesChanged,
                    this, std::placeholders::_1));

            _dtcInformation.SetCurrentStatus(cDtc, cMask, _statusByte);
            EXPECT_EQ(cExpectedAddedResult, NotifiedNumberOfStoredEntries);

            core::Result<void> _result{_dtcInformation.Clear(cDtc)};
            EXPECT_TRUE(_result.HasValue());
            EXPECT_EQ(cExpectedClearedResult, NotifiedNumberOfStoredEntries);
        }

        TEST_F(DtcInformationTest, MaskedOutStatusChange)
        {
            const uint32_t cDtc{1};
            const uint8_t cInitialStatus{0x01};
            const uint8_t cNewStatus{0x09};
            const uint32_t cExpectedCallCount{0};

            UdsDtcStatusByteType _statusByte;
            _statusByte.encodedBits = cInitialStatus;

            DTCInformation _dtcInformation(Specifier);
            _dtcInformation.SetCurrentStatus(
                cDtc, UdsDtcStatusBitType::kTestFailed, _statusByte);

            uint32_t _callCount{0};
            std::function<void(uint32_t, UdsDtcStatusByteType, UdsDtcStatusByteType)> _notifier{
                [&_callCount](uint32_t, UdsDtcStatusByteType, UdsDtcStatusByteType)
                {
                    ++_callCount;
                }};
            _dtcInformation.SetDTCStatusChangedNotifier(_notifier);

            // The changed bit (confirmed DTC) is not part of the mask (test failed)
            _statusByte.encodedBits = cNewStatus;
            _dtcInformation.SetCurrentStatus(
                cDtc, UdsDtcStatusBitType::kTestFailed, _statusByte);

            EXPECT_EQ(cExpectedCallCount, _callCount);
            EXPECT_EQ(
                cInitialStatus,
                _dtcInformation.GetCurrentStatus(cDtc).Value().encodedBits);
        }

        TEST_F(DtcInformationTest, EnableControlDtcNotifiesOnlyOnce)
        {
            const uint32_t cExpectedCallCount{1};

            uint32_t _callCount{0};
            std::function<void(ControlDtcStatusType)> _notifier{
                [&_callCount](ControlDtcStatusType)
                {
                    ++_callCount;
                }};

            DTCInformation _dtcInformation(Specifier);
            _dtcInformation.SetControlDtcStatusNotifier(_notifier);
            _dtcInformation.EnableControlDtc();
            _dtcInformation.EnableControlDtc();

            EXPECT_EQ(cExpectedCallCount, _callCount);
            EXPECT_EQ(
                ControlDtcStatusType::kDTCSettingOn,
                _dtcInformation.GetControlDTCStatus().Value());
        }
    }
}
