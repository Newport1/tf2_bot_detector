#include "Config/ConfigHelpers.h"

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

using namespace tf2_bot_detector;

TEST_CASE("ConfigSchemaInfo owner alternation", "[tf2bd][config]")
{
	const ConfigSchemaInfo pazer{
		"https://raw.githubusercontent.com/PazerOP/tf2_bot_detector/master/schemas/v3/playerlist.schema.json" };
	REQUIRE(pazer.m_Branch == "master");
	REQUIRE(pazer.m_Version == 3u);
	REQUIRE(pazer.m_Type == "playerlist");

	const ConfigSchemaInfo surepy{
		"https://raw.githubusercontent.com/surepy/tf2_bot_detector/master/schemas/v3/rules.schema.json" };
	REQUIRE(surepy.m_Branch == "master");
	REQUIRE(surepy.m_Version == 3u);
	REQUIRE(surepy.m_Type == "rules");

	const ConfigSchemaInfo newport{
		"https://raw.githubusercontent.com/Newport1/tf2_bot_detector/custom/schemas/v3/playerlist.schema.json" };
	REQUIRE(newport.m_Branch == "custom");
	REQUIRE(newport.m_Version == 3u);
	REQUIRE(newport.m_Type == "playerlist");

	REQUIRE_THROWS_AS(
		ConfigSchemaInfo("https://raw.githubusercontent.com/UnknownOwner/tf2_bot_detector/master/schemas/v3/playerlist.schema.json"),
		std::runtime_error);
}
