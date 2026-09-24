// doctest coverage for the control socket's local console-command adapter.
//
// Run: ctest --test-dir <build directory> --build-config Release -R "Control console"

#include "doctest.h"

#include "App/Control/ConsoleCommand.h"

#include <stdexcept>
#include <string>

namespace
{
bool ConsumeCommand(const std::wstring& command)
{
    return command == L"$effects sprites off";
}

bool RejectCommand(const std::wstring&)
{
    return false;
}

bool ThrowForMalformedCommand(const std::wstring&)
{
    throw std::invalid_argument("invalid argument");
}

bool parserCalled = false;

bool RecordParserCall(const std::wstring&)
{
    parserCalled = true;
    return true;
}
} // namespace

TEST_CASE("Control console passes recognized commands to its parser [network][control-console]")
{
    CHECK(App::Control::Console::Execute(L"$effects sprites off", ConsumeCommand) ==
          App::Control::Console::Result::Consumed);
}

TEST_CASE("Control console reports unrecognized parser input [network][control-console]")
{
    CHECK(App::Control::Console::Execute(L"$not-a-console-command", RejectCommand) ==
          App::Control::Console::Result::Unrecognized);
}

TEST_CASE("Control console contains malformed parser input [network][control-console]")
{
    CHECK(App::Control::Console::Execute(L"$fps invalid", ThrowForMalformedCommand) ==
          App::Control::Console::Result::Invalid);
}

#ifndef CSK_LH_DEBUG_CONSOLE
TEST_CASE("Control console identifies Debug-only commands in Release [network][control-console]")
{
    parserCalled = false;

    CHECK(App::Control::Console::Execute(L"$open", RecordParserCall) == App::Control::Console::Result::DebugOnly);
    CHECK_FALSE(parserCalled);
}
#endif
