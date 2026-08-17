#include "Config/PlayerListJSON.h"

#include <catch2/catch_test_macros.hpp>

using namespace std::chrono_literals;
using namespace tf2_bot_detector;

TEST_CASE("mh::formatter<PlayerAttributesList>", "[tf2bd][formatting]")
{
	PlayerAttributesList list;

	list.SetAttribute(PlayerAttribute::Cheater);
	// {:v} (a92cf5b) prints value-only, not PlayerAttribute::Cheater
	REQUIRE(fmt::format("Attribs: {}", list) == "Attribs: Cheater");
}

TEST_CASE("mh::formatter<PlayerMarks>", "[tf2bd][formatting]")
{
	PlayerMarks marks;
	marks.m_Marks.push_back(PlayerMarks::Mark({ PlayerAttribute::Suspicious, PlayerAttribute::Racist }, "cfg/playerlist.json"));

	auto fmt = fmt::format("marks: {}", marks);
	// {:v} (a92cf5b) prints value-only; Mark formats the attributes list via "{}"
	REQUIRE(fmt == "marks: \n\t - \"cfg/playerlist.json\" (Suspicious, Racist)");
}
