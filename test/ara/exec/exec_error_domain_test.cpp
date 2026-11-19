#include <string>
#include <utility>
#include <vector>
#include <gtest/gtest.h>
#include "../../../src/ara/core/error_code.h"
#include "../../../src/ara/exec/exec_error_domain.h"

namespace ara
{
    namespace exec
    {
        TEST(ExecErrorDomainTest, IdProperty)
        {
            const core::ErrorDomain::IdType cExpectedResult{0x8000000000000202};
            ExecErrorDomain _errorDomain;

            EXPECT_EQ(cExpectedResult, _errorDomain.Id());
        }

        TEST(ExecErrorDomainTest, NameProperty)
        {
            const std::string cExpectedResult{"Execution Management error domain"};
            ExecErrorDomain _errorDomain;

            EXPECT_EQ(cExpectedResult, _errorDomain.Name());
        }

        TEST(ExecErrorDomainTest, Equality)
        {
            ExecErrorDomain _errorDomain;
            ExecErrorDomain _otherErrorDomain;

            EXPECT_TRUE(_errorDomain == _otherErrorDomain);
        }

        TEST(ExecErrorDomainTest, MessageMethod)
        {
            const std::vector<std::pair<ExecErrc, std::string>> cExpectedMessages{
                {ExecErrc::kGeneralError,
                 "Unspecified error occurrence"},
                {ExecErrc::kInvalidArguments,
                 "Invalid argument while setting the state"},
                {ExecErrc::kCommunicationError,
                 "SOME/IP or FIFO communication error occurrence"},
                {ExecErrc::kMetaModelError,
                 "Invalid ARXML meta-model"},
                {ExecErrc::kCancelled,
                 "Cancelling of the on-going state transition due to a new state trainsition request"},
                {ExecErrc::kFailed,
                 "Impossible operation to be performed"},
                {ExecErrc::kFailedUnexpectedTerminationOnExit,
                 "Previous function group termination while state transition"},
                {ExecErrc::kFailedUnexpectedTerminationOnEnter,
                 "Target function group termination while state transition"},
                {ExecErrc::kInvalidTransition,
                 "Invalid state transition request"},
                {ExecErrc::kAlreadyInState,
                 "Invalid state transition due to request the current state"},
                {ExecErrc::kInTransitionToSameState,
                 "Transition failing due to equality to the on-going state transition"},
                {ExecErrc::kNoTimeStamp,
                 "Unavailable deterministic timestamp information"},
                {ExecErrc::kCycleOverrun,
                 "Exceeding theoritical cycle time"}};
            ExecErrorDomain _errorDomain;

            for (const std::pair<ExecErrc, std::string> &expectedMessage : cExpectedMessages)
            {
                core::ErrorDomain::CodeType _errorCode{
                    static_cast<core::ErrorDomain::CodeType>(expectedMessage.first)};
                std::string _actualResult{_errorDomain.Message(_errorCode)};

                EXPECT_EQ(expectedMessage.second, _actualResult);
            }
        }

        TEST(ExecErrorDomainTest, UnsupportedErrorCodeMessage)
        {
            const std::string cExpectedResult{"Unsupported error code"};
            const core::ErrorDomain::CodeType cZeroErrorCode{0};
            const core::ErrorDomain::CodeType cOutOfRangeErrorCode{14};
            ExecErrorDomain _errorDomain;

            std::string _actualResult{_errorDomain.Message(cZeroErrorCode)};
            EXPECT_EQ(cExpectedResult, _actualResult);

            _actualResult = _errorDomain.Message(cOutOfRangeErrorCode);
            EXPECT_EQ(cExpectedResult, _actualResult);
        }

        TEST(ExecErrorDomainTest, ErrorCodeMessage)
        {
            const std::string cExpectedResult{"Invalid state transition request"};
            const ExecErrc cExecErrc{ExecErrc::kInvalidTransition};
            ExecErrorDomain _errorDomain;

            core::ErrorCode _errorCode{
                static_cast<core::ErrorDomain::CodeType>(cExecErrc), _errorDomain};

            EXPECT_EQ(cExpectedResult, _errorCode.Message());
        }
    }
}
