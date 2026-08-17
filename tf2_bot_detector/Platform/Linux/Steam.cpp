#include "Platform/Platform.h"
#include "Util/PathUtils.h"
#include "Util/TextUtils.h"
#include "Log.h"

#include "SteamID.h"

#include <vdf_parser.hpp>
#include <mh/text/string_insertion.hpp>

#include <cstdlib>
#include <optional>
#include <pwd.h>
#include <unistd.h>

static std::filesystem::path ResolveUserHomeDirectory()
{
	if (const char* home = std::getenv("HOME"); home && *home)
		return std::filesystem::path(home);

	if (const passwd* pw = getpwuid(getuid()); pw && pw->pw_dir && *pw->pw_dir)
		return std::filesystem::path(pw->pw_dir);

	return {};
}

static std::optional<std::filesystem::path> EnvPath(const char* name)
{
	if (const char* value = std::getenv(name); value && *value)
		return std::filesystem::path(value);
	return std::nullopt;
}

std::filesystem::path tf2_bot_detector::Platform::GetCurrentSteamDir()
{
	const auto home = ResolveUserHomeDirectory();
	const auto steamRoot = EnvPath("STEAM_ROOT");
	const auto steamBaseFolder = EnvPath("STEAM_BASE_FOLDER");

	if (auto found = DiscoverSteamDir(home, steamRoot, steamBaseFolder); !found.empty())
		return found;

	// Settings::m_SteamDirOverride is applied by Settings::GetSteamDir(); this
	// function is autodetection only and must not invent a second override field.
	LogError(MH_SOURCE_LOCATION_CURRENT(),
		"Unable to locate a Steam directory containing steamapps/. Tried, in order: "
		"$STEAM_ROOT ({}), $STEAM_BASE_FOLDER ({}), {}/.steam/steam, {}/.local/share/Steam, "
		"{}/.var/app/com.valvesoftware.Steam/.local/share/Steam, {}/.steam/root. "
		"Set STEAM_ROOT or choose the Steam folder in TF2BD settings.",
		steamRoot ? steamRoot->string() : std::string("<unset>"),
		steamBaseFolder ? steamBaseFolder->string() : std::string("<unset>"),
		home.string(), home.string(), home.string(), home.string());
	return {};
}

// TODO: paste from msb people they probably figured out a detection method
// pasted from https://github.com/MegaAntiCheat/client-backend/blob/9714449f7cc1845200df5c537ca8d42d8eeb6c3d/src/settings.rs#L168-L205
// TODO: venture for a better detection method?
// FIXME: this doesn't follow the steamid folder that has been overwritten by settings, it should probably do that instead.
tf2_bot_detector::SteamID tf2_bot_detector::Platform::GetCurrentActiveSteamID()
{
	std::filesystem::path loginUsers = GetCurrentSteamDir() / "config" / "loginusers.vdf";

	SteamID returnUser;

	try
	{
		std::ifstream file;
		file.exceptions(std::ios::badbit | std::ios::failbit);
		file.open(loginUsers);
		tyti::vdf::object LoginUserRoot = tyti::vdf::read(file);

		assert(LoginUserRoot.name == "users");

		time_t last_time = 0;

		for (const auto& [steamidStr, accountEntry] : LoginUserRoot.childs) {
			time_t timestamp = std::stol(accountEntry.get()->attribs.at("Timestamp"));

			if (last_time == 0 || timestamp > last_time) {
				returnUser = SteamID(steamidStr);
				last_time = timestamp;
			}
		}
	}
	catch (...) {
		// do error stuff
	}


	return returnUser;
}
