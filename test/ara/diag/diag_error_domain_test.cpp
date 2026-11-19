#include <gtest/gtest.h>
#include <map>
#include <string>
#include "../../../src/ara/diag/diag_error_domain.h"

namespace ara
{
    namespace diag
    {
        TEST(DiagErrorDomainTest, NameProperty)
        {
            ara::core::ErrorDomain *_errorDomain{DiagErrorDomain::GetDiagDomain()};
            EXPECT_STRNE(_errorDomain->Name(), "");
        }

        TEST(DiagErrorDomainTest, MakeErrorCodeMethod)
        {
            const DiagErrc cExpectedResult{DiagErrc::kGenericError};

            ara::core::ErrorDomain *_errorDomain{DiagErrorDomain::GetDiagDomain()};
            auto _diagErrorDomain{static_cast<DiagErrorDomain *>(_errorDomain)};
            ara::core::ErrorCode _errorCode{_diagErrorDomain->MakeErrorCode(cExpectedResult)};
            auto _actualResult{static_cast<DiagErrc>(_errorCode.Value())};

            EXPECT_EQ(cExpectedResult, _actualResult);
        }

        TEST(DiagErrorDomainTest, MessageMethod)
        {
            const std::map<DiagErrc, std::string> cExpectedMessages{
                {DiagErrc::kAlreadyOffered, "Already offered service"},
                {DiagErrc::kConfigurationMismatch, "Configuration misalignment with DEXT"},
                {DiagErrc::kDebouncingConfigurationInconsistent, "Invalid monitor debouncing configuration"},
                {DiagErrc::kReportIgnored, "Disabled control DTC setting"},
                {DiagErrc::kInvalidArgument, "Invalid passed argument from caller"},
                {DiagErrc::kNotOffered, "Request from a not offered service"},
                {DiagErrc::kGenericError, "General error occurrance"},
                {DiagErrc::kNoSuchDTC, "Invalid DTC number"},
                {DiagErrc::kBusy, "Busy interface call"},
                {DiagErrc::kFailed, "Failed process"},
                {DiagErrc::kMemoryError, "Memory error occurrance"},
                {DiagErrc::kWrongDtc, "Incorrect passed DTC number"},
                {DiagErrc::kRejected, "Request rejection"},
                {DiagErrc::kResetTypeNotSupported, "Unsupported reset type by the Diagnostic Address instance"},
                {DiagErrc::kRequestFailed, "Failed diagnostic request process"}};

            ara::core::ErrorDomain *_errorDomain{DiagErrorDomain::GetDiagDomain()};

            for (const std::pair<const DiagErrc, std::string> &_expectedMessage : cExpectedMessages)
            {
                ara::core::ErrorDomain::CodeType _code{
                    static_cast<ara::core::ErrorDomain::CodeType>(_expectedMessage.first)};

                EXPECT_STREQ(
                    _expectedMessage.second.c_str(), _errorDomain->Message(_code));
            }
        }

        TEST(DiagErrorDomainTest, UnsupportedMessage)
        {
            const ara::core::ErrorDomain::CodeType cUnsupportedCode{0};

            ara::core::ErrorDomain *_errorDomain{DiagErrorDomain::GetDiagDomain()};

            EXPECT_STREQ("Not supported", _errorDomain->Message(cUnsupportedCode));
        }
    }
}