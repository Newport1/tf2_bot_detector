#include "Config/ConfigHelpers.h"
#include "Config/PlayerListJSON.h"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string>
#include <utility>

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

TEST_CASE("Third-party configs are not normalized after a successful load", "[tf2bd][config]")
{
	REQUIRE(detail::ShouldNormalizeConfigOnLoad("cfg/playerlist.json"));
	REQUIRE(detail::ShouldNormalizeConfigOnLoad("cfg/playerlist.official.json"));
	REQUIRE(detail::ShouldNormalizeConfigOnLoad("cfg/PLAYERLIST.OFFICIAL.JSON"));

	REQUIRE_FALSE(detail::ShouldNormalizeConfigOnLoad("cfg/playerlist.steamrep.json"));
	REQUIRE_FALSE(detail::ShouldNormalizeConfigOnLoad("cfg/rules.community.extra.json"));
}

TEST_CASE("Large third-party player lists deserialize and count entries", "[tf2bd][config][playerlist]")
{
	constexpr size_t playerCount = 5000;
	nlohmann::json players = nlohmann::json::array();

	for (size_t i = 1; i <= playerCount; ++i)
	{
		players.push_back({
			{ "steamid", "[U:1:" + std::to_string(i) + "]" },
			{ "attributes", nlohmann::json::array({ "suspicious" }) },
		});
	}

	PlayerMap_t largeList;
	detail::DeserializePlayerListEntries(players, largeList);

	REQUIRE(largeList.size() == playerCount);
	REQUIRE(largeList.begin()->second.m_SavedAttributes.HasAttribute(PlayerAttribute::Suspicious));

	ThirdPartyPlayerLists_t lists;
	lists.emplace_back("large", std::move(largeList));

	PlayerMap_t smallList;
	const SteamID extraID("[U:1:900000]");
	smallList.emplace(extraID, PlayerListData(extraID));
	lists.emplace_back("small", std::move(smallList));

	REQUIRE(detail::CountThirdPartyPlayerListEntries(lists) == playerCount + 1);
}
