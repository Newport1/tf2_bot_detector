#include "Platform/Platform.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace tf2_bot_detector;

namespace
{
	void RequireTokens(std::string_view input, const std::vector<std::string>& expected)
	{
		const auto got = Shell::SplitCommandLineArgs(input);
		REQUIRE(got == expected);
	}
}

using Case = std::pair<std::string_view, std::vector<std::string>>;

// Cases where both tokenizers agree. Every input here starts with a plain,
// unquoted token on purpose -- see the argv[0] note below.
TEST_CASE("SplitCommandLineArgs - table", "[tf2bd][shell]")
{
	const Case cases[] = {
		{ "-novid -console", { "-novid", "-console" } },
		{ "-condebug \"C:\\Program Files\\x\"", { "-condebug", "C:\\Program Files\\x" } },
		{ "foo \"bar \\\"baz\\\"\"", { "foo", "bar \"baz\"" } },
		{ "-a    \t  -b", { "-a", "-b" } },
		{ "  -novid -console  ", { "-novid", "-console" } },
	};

	for (const auto& [input, expected] : cases)
		RequireTokens(input, expected);
}

// The two implementations are genuinely different tokenizers, not one behaviour
// with a bug on one side:
//   Linux   -- Platform/Linux/Shell.cpp, a POSIX-ish parser: '...' quotes, and
//              backslash escapes a following space.
//   Windows -- Platform/Windows/Shell.cpp, CommandLineToArgvW: ' is an ordinary
//              character, and \ is only special immediately before a ".
// Asserting one platform's rules on the other is what made this file fail to
// port, so the divergent cases live here, per platform, deliberately.
#ifdef _WIN32
TEST_CASE("SplitCommandLineArgs - Windows (CommandLineToArgvW) quoting", "[tf2bd][shell]")
{
	const Case cases[] = {
		// Single quotes are not quoting characters; they stay in the tokens.
		{ "'quoted value'", { "'quoted", "value'" } },
		// A backslash before a space is literal, so the space still separates.
		{ "plain\\ space", { "plain\\", "space" } },
	};

	for (const auto& [input, expected] : cases)
		RequireTokens(input, expected);
}
#else
TEST_CASE("SplitCommandLineArgs - POSIX quoting", "[tf2bd][shell]")
{
	const Case cases[] = {
		{ "'quoted value'", { "quoted value" } },
		{ "plain\\ space", { "plain space" } },
		// A bare quoted empty first token. Deliberately NOT asserted on Windows:
		// SplitCommandLineArgs hands the raw string to CommandLineToArgvW without
		// prepending a program name (Platform/Windows/Shell.cpp:123), so the first
		// token is parsed under argv[0] rules, where quote handling differs and the
		// empty-command-line case is special-cased by the API. The non-leading form
		// is covered for both platforms by the "empty quoted token" case below.
		{ "\"\"", { "" } },
	};

	for (const auto& [input, expected] : cases)
		RequireTokens(input, expected);
}
#endif

TEST_CASE("SplitCommandLineArgs - unterminated quote", "[tf2bd][shell]")
{
	const auto tokens = Shell::SplitCommandLineArgs("-ok \"still open");
	REQUIRE_FALSE(tokens.empty());
	REQUIRE(tokens.front() == "-ok");
	REQUIRE(tokens.back() == "still open");
}

TEST_CASE("SplitCommandLineArgs - empty quoted token is preserved", "[tf2bd][shell]")
{
	RequireTokens("a \"\" b", { "a", "", "b" });
}
