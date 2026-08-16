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

TEST_CASE("SplitCommandLineArgs - table", "[tf2bd][shell]")
{
	using Case = std::pair<std::string_view, std::vector<std::string>>;

	const Case cases[] = {
		{ "-novid -console", { "-novid", "-console" } },
		{ "-condebug \"C:\\Program Files\\x\"", { "-condebug", "C:\\Program Files\\x" } },
		{ "foo \"bar \\\"baz\\\"\"", { "foo", "bar \"baz\"" } },
		{ "\"\"", { "" } },
		{ "-a    \t  -b", { "-a", "-b" } },
		{ "  -novid -console  ", { "-novid", "-console" } },
		{ "'quoted value'", { "quoted value" } },
		{ "plain\\ space", { "plain space" } },
	};

	for (const auto& [input, expected] : cases)
		RequireTokens(input, expected);
}

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
