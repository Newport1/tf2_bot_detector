#include "PathUtils.h"
#include "Log.h"
#include "Platform/Platform.h"

#include <mh/text/string_insertion.hpp>
#include <vdf_parser.hpp>
#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <string>
#include <unordered_set>
#include <vector>

#ifndef _WIN32
#include <unistd.h>
#endif

using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace tf2_bot_detector;

#ifdef _WIN32
// On-disk game binaries. Same three names as kTF2ProcessNames: WMI
// Win32_Process.Name is a basename and the comparison is case-insensitive,
// so the executable list and the process list happen to coincide on Windows.
// Keep the arrays separate anyway — they answer different questions.
static constexpr std::string_view kTF2ExecutableNames[] = {
	"tf_win64.exe",
	"tf.exe",
	"hl2.exe",
};
static constexpr std::string_view kTF2ProcessNames[] = {
	"tf_win64.exe",
	"tf.exe",
	"hl2.exe",
};
#else
// On-disk game binaries, including tf.sh (the current Linux launch wrapper
// used by Settings::GetBinaryName). Distinct from kTF2ProcessNames.
static constexpr std::string_view kTF2ExecutableNames[] = {
	"tf_linux64",
	"tf.sh",
	"hl2_linux",
};
// Live-process names only. tf.sh is a launcher shell script; the running
// game is tf_linux64. Folding this into TF2ExecutableNames() would let
// GetTF2CommandLineArgsAsync latch onto the wrapper pid, whose
// /proc/<pid>/cmdline is the script rather than the game.
static constexpr std::string_view kTF2ProcessNames[] = {
	"tf_linux64",
	"hl2_linux",
};
#endif

[[nodiscard]] static bool ValidateDirectory(DirectoryValidatorResult& result, const std::string_view& relative)
{
	using Result = DirectoryValidatorResult::Result;

	const auto fullPath = result.m_Path / relative;
	if (!std::filesystem::exists(fullPath))
	{
		result.m_Message = "Expected folder "s << std::quoted(relative) << " does not exist";
		result.m_Result = Result::InvalidContents;
		return false;
	}

	auto canonical = std::filesystem::canonical(fullPath);
	if (!std::filesystem::is_directory(canonical))
	{
		result.m_Message = "Expected folder "s << std::quoted(relative) << " is not a folder";
		result.m_Result = Result::InvalidContents;
		return false;
	}

	return true;
}

[[nodiscard]] static bool ValidateFile(DirectoryValidatorResult& result, const std::string_view& relative)
{
	using Result = DirectoryValidatorResult::Result;

	const auto fullPath = result.m_Path / relative;
	if (!std::filesystem::exists(fullPath))
	{
		result.m_Message = "Expected file "s << std::quoted(relative) << " does not exist";
		result.m_Result = Result::InvalidContents;
		return false;
	}

	auto canonical = std::filesystem::canonical(fullPath);
	if (!std::filesystem::is_regular_file(canonical))
	{
		result.m_Message = "Expected file "s << std::quoted(relative) << " is not a file";
		result.m_Result = Result::InvalidContents;
		return false;
	}

	return true;
}

[[nodiscard]] static bool BasicDirChecks(DirectoryValidatorResult& result)
{
	using Result = DirectoryValidatorResult::Result;

	if (!std::filesystem::exists(result.m_Path))
	{
		result.m_Result = Result::DoesNotExist;
		result.m_Message = "Path does not exist";
		return false;
	}

	result.m_Path = std::filesystem::canonical(result.m_Path);
	if (!std::filesystem::is_directory(result.m_Path))
	{
		result.m_Result = Result::NotADirectory;
		result.m_Message = "Not a folder";
		return false;
	}

	return true;
}


DirectoryValidatorResult tf2_bot_detector::ValidateTFDir(std::filesystem::path path)
{
	if (path.empty())
		LogError(MH_SOURCE_LOCATION_CURRENT(), "\"path\" was empty!");

	DirectoryValidatorResult result(std::move(path));

	using Result = DirectoryValidatorResult::Result;

	try
	{
		if (!BasicDirChecks(result))
			return result;

		const bool tf2_binary_validate_result = FindTF2Executable(result.m_Path).has_value();

		if (tf2_binary_validate_result)
		{
			result.m_Result = Result::Valid;
			result.m_Message.clear();
		}
		else
		{
			result.m_Result = Result::InvalidContents;
			result.m_Message = "No recognized TF2 executable found next to this tf/ folder";
		}

		if (!tf2_binary_validate_result ||
			!ValidateFile(result, "tf2_misc_dir.vpk") ||
			!ValidateFile(result, "tf2_sound_misc_dir.vpk") ||
			!ValidateFile(result, "tf2_textures_dir.vpk") ||
			!ValidateDirectory(result, "custom"))
		{
			return result;
		}
	}
	catch (const std::filesystem::filesystem_error& e)
	{
		result.m_Result = Result::FilesystemError;
		result.m_Message = e.what();
	}

	return result;
}

DirectoryValidatorResult tf2_bot_detector::ValidateSteamDir(std::filesystem::path path)
{
	DirectoryValidatorResult result(std::move(path));

	using Result = DirectoryValidatorResult::Result;

	try
	{
		if (!BasicDirChecks(result))
			return result;

		// gameoverlayui.exe or gameoverlayui binary
		bool game_overlay_result = ValidateFile(result, STEAM_BIN_DIR(PLATFORM_EXECUTABLE("gameoverlayui")));

		// some people dont have a gameoverlayui.exe and only have a gameoverlayui64.exe, and i don't know why
		// gameoverlayui67
		if (!game_overlay_result) {
			game_overlay_result = ValidateFile(result, STEAM_BIN_DIR(PLATFORM_EXECUTABLE("gameoverlayui64")));
		}

		// FIXME2: this is kind of a bad fix, but fuck do i care
		if (game_overlay_result) {
			result.m_Result = Result::Valid;
			result.m_Message.clear();
		}

		if (!ValidateFile(result, STEAM_BIN_DIR(PLATFORM_EXECUTABLE("steam"))) ||
			!ValidateFile(result, STEAM_BIN_DIR(PLATFORM_EXECUTABLE("streaming_client"))) ||
			!game_overlay_result ||
			!ValidateDirectory(result, "steamapps") ||
			!ValidateDirectory(result, "config"))
		{
			return result;
		}
	}
	catch (const std::filesystem::filesystem_error& e)
	{
		result.m_Result = Result::FilesystemError;
		result.m_Message = e.what();
	}

	return result;
}

static bool IsNumericKey(const std::string& key)
{
	return !key.empty() && std::all_of(key.begin(), key.end(), [](unsigned char c) { return std::isdigit(c); });
}

static std::filesystem::path NormalizeLibrarySteamApps(const std::filesystem::path& libraryRoot)
{
	const auto steamapps = libraryRoot / "steamapps";
	std::error_code ec;
	auto normalized = std::filesystem::weakly_canonical(steamapps, ec);
	if (ec)
		normalized = steamapps.lexically_normal();
	return normalized;
}

mh::generator<std::filesystem::path> tf2_bot_detector::GetSteamLibraryFolders(const std::filesystem::path& steamDir)
{
	const auto baseSteamAppsDir = steamDir / "steamapps";
	if (!std::filesystem::exists(baseSteamAppsDir))
		co_return;

	std::unordered_set<std::string> seen;
	auto consider = [&](const std::filesystem::path& libraryRoot) -> std::optional<std::filesystem::path>
	{
		if (libraryRoot.empty())
			return std::nullopt;

		const auto normalized = NormalizeLibrarySteamApps(libraryRoot);
		if (!seen.insert(normalized.generic_string()).second)
			return std::nullopt;
		return normalized;
	};

	if (auto base = consider(steamDir))
		co_yield *base;

	const auto libraryFoldersFilename = baseSteamAppsDir / "libraryfolders.vdf";
	if (!std::filesystem::exists(libraryFoldersFilename))
	{
		DebugLogWarning(MH_SOURCE_LOCATION_CURRENT(), "{} does not exist", libraryFoldersFilename);
		co_return;
	}

	std::ifstream file(libraryFoldersFilename);
	if (!file.good())
	{
		DebugLogWarning(MH_SOURCE_LOCATION_CURRENT(), "Failed to read {}", libraryFoldersFilename);
		co_return;
	}

	try
	{
		std::error_code ec;
		auto vdf = tyti::vdf::read(file, ec);
		if (ec)
		{
			LogWarning(MH_SOURCE_LOCATION_CURRENT(), "Failed to parse {}: {}", libraryFoldersFilename, ec.message());
			co_return;
		}

		const tyti::vdf::object* root = &vdf;
		if (auto it = vdf.childs.find("libraryfolders"); it != vdf.childs.end() && it->second)
			root = it->second.get();
		else if (auto it = vdf.childs.find("LibraryFolders"); it != vdf.childs.end() && it->second)
			root = it->second.get();

		// Legacy: numeric attribute → value is the library path.
		for (const auto& attrib : root->attribs)
		{
			if (!IsNumericKey(attrib.first))
				continue;
			if (auto path = consider(std::filesystem::path(attrib.second)))
				co_yield *path;
		}

		// Modern: numeric child object → child.attribs["path"] is the library path.
		for (const auto& [key, child] : root->childs)
		{
			if (!IsNumericKey(key) || !child)
				continue;

			const auto pathIt = child->attribs.find("path");
			if (pathIt == child->attribs.end() || pathIt->second.empty())
				continue;

			if (auto path = consider(std::filesystem::path(pathIt->second)))
				co_yield *path;
		}
	}
	catch (const std::exception& e)
	{
		LogWarning(MH_SOURCE_LOCATION_CURRENT(), "Ignoring malformed {}: {}", libraryFoldersFilename, e.what());
	}
}

std::filesystem::path tf2_bot_detector::FindTFDir(const std::filesystem::path& steamDir)
{
	for (const auto& libraryFolder : GetSteamLibraryFolders(steamDir))
	{
		auto tfDir = libraryFolder / "common" / "Team Fortress 2" / "tf";
		if (!ValidateTFDir(tfDir))
			continue;

		return tfDir;
	}

	DebugLog(MH_SOURCE_LOCATION_CURRENT(), "Failed to find tf directory from {}", steamDir);
	return {};
}

std::filesystem::path tf2_bot_detector::FindSteamLinuxRuntimeSniper(const std::filesystem::path& steamDir)
{
	for (const auto& libraryFolder : GetSteamLibraryFolders(steamDir))
	{
		const auto runPath = libraryFolder / "common" / "SteamLinuxRuntime_sniper" / "run";
		if (!IsSteamLinuxRuntimeSniperUsable(runPath))
			continue;

		return runPath;
	}

	DebugLog(MH_SOURCE_LOCATION_CURRENT(), "Failed to find SteamLinuxRuntime_sniper from {}", steamDir);
	return {};
}

std::span<const std::string_view> tf2_bot_detector::TF2ExecutableNames()
{
	return kTF2ExecutableNames;
}

std::span<const std::string_view> tf2_bot_detector::TF2ProcessNames()
{
	return kTF2ProcessNames;
}

std::optional<std::filesystem::path> tf2_bot_detector::FindTF2Executable(const std::filesystem::path& tf_dir)
{
	if (tf_dir.empty())
		return std::nullopt;

	for (const auto name : TF2ExecutableNames())
	{
		const auto candidate = tf_dir / ".." / name;
		std::error_code ec;
		if (!std::filesystem::is_regular_file(candidate, ec) || ec)
			continue;

		auto canonical = std::filesystem::weakly_canonical(candidate, ec);
		return ec ? candidate.lexically_normal() : canonical;
	}

	return std::nullopt;
}

std::filesystem::path tf2_bot_detector::DiscoverSteamDir(
	const std::filesystem::path& home,
	std::optional<std::filesystem::path> steamRoot,
	std::optional<std::filesystem::path> steamBaseFolder)
{
	std::vector<std::filesystem::path> candidates;
	candidates.reserve(6);

	if (steamRoot && !steamRoot->empty())
		candidates.push_back(*steamRoot);
	if (steamBaseFolder && !steamBaseFolder->empty())
		candidates.push_back(*steamBaseFolder);

	if (!home.empty())
	{
		candidates.push_back(home / ".steam" / "steam");
		candidates.push_back(home / ".local" / "share" / "Steam");
		candidates.push_back(home / ".var" / "app" / "com.valvesoftware.Steam" / ".local" / "share" / "Steam");
		candidates.push_back(home / ".steam" / "root");
	}

	for (const auto& candidate : candidates)
	{
		std::error_code ec;
		if (!std::filesystem::exists(candidate, ec) || ec)
			continue;

		const auto steamapps = candidate / "steamapps";
		if (!std::filesystem::is_directory(steamapps, ec) || ec)
			continue;

		auto canonical = std::filesystem::weakly_canonical(candidate, ec);
		return ec ? candidate : canonical;
	}

	return {};
}

// Detection only. TF2BD never adds this flag -- it is matched solely to notice
// that the *user* already put it in their own Steam launch options, so we don't
// contradict them by appending -secure afterwards.
static constexpr std::string_view kUserInsecureFlag = "-insecure"; // safety-boundary-allow

// Whole-token search, so a longer token or a path containing the text doesn't match.
static bool HasWholeFlag(std::string_view args, std::string_view flag)
{
	const auto isSep = [](char c) { return c == ' ' || c == '\t'; };

	for (size_t pos = args.find(flag); pos != std::string_view::npos; pos = args.find(flag, pos + flag.size()))
	{
		const size_t end = pos + flag.size();
		if ((pos == 0 || isSep(args[pos - 1])) && (end == args.size() || isSep(args[end])))
			return true;
	}

	return false;
}

static std::string BuildTF2RequiredLaunchArgs(std::string_view rconPassword, uint16_t rconPort,
	bool userDisabledVAC)
{
	// TF2BD asks for -secure by default, but the user's own launch options win.
	// Appending -secure after their explicit opt-out would silently override a
	// deliberate choice on their own machine.
	const std::string_view secureArg = userDisabledVAC ? " -steam" : " -steam -secure";

	return fmt::format(
		// Sacrificial token -- keep it. The user's own launch options are prepended
		// verbatim, so if theirs ends with a flag that expects a value, that flag
		// swallows whatever comes next. This is what gets swallowed instead of
		// "-game tf". Nothing reads it because nothing is meant to.
		// Upstream called it " dummy" ("Dummy option in case user has mismatched
		// command line args in their steam config"); surepy renamed it to " bd" in
		// fa99c91 and kept the behaviour. The comment was lost when this function was
		// extracted in 26884bc, which is what made it look like stray debris.
		" bd"
		" -game tf"
		"{}"
		" -usercon"
		" +developer 1"
		" +ip 0.0.0.0"
		" +sv_rcon_whitelist_address 127.0.0.1"
		" +sv_quota_stringcmdspersecond 1000000"
		" +rcon_password {}"
		" +hostport {}"
		" +net_start"
		" +con_timestamp 1"
		" -condebug"
		" -conclearlog",
		secureArg, rconPassword, rconPort);
}

TF2LaunchArgsPlan tf2_bot_detector::PlanTF2LaunchArgs(
	std::string userArgs,
	std::string_view rconPassword,
	uint16_t rconPort,
	bool useRecommendedParams)
{
	const std::string originalUserArgs = userArgs;
	const bool userDisabledVAC = HasWholeFlag(originalUserArgs, kUserInsecureFlag);
	userArgs += BuildTF2RequiredLaunchArgs(rconPassword, rconPort, userDisabledVAC);

	TF2LaunchArgsPlan plan;
	plan.limit = kTF2LaunchArgsHardLimit;

	const bool canFitRecommended = useRecommendedParams
		&& userArgs.size() <= kTF2LaunchArgsLimitWithRecommended;

	if (canFitRecommended)
		userArgs += kTF2RecommendedLaunchArgs;

	plan.args = std::move(userArgs);
	plan.length = plan.args.size();

	if (plan.length > kTF2LaunchArgsHardLimit)
	{
		plan.action = TF2LaunchArgsAction::Refuse;
		plan.error = fmt::format(
			"TF2 launch arguments are {} characters (limit {}). "
			"Even after dropping recommended aliases, the command line will not fit. "
			"Shorten your Steam launch options for Team Fortress 2 and try again. "
			"Current user launch options: \"{}\"",
			plan.length, kTF2LaunchArgsHardLimit, originalUserArgs);
		return plan;
	}

	if (useRecommendedParams && !canFitRecommended)
		plan.action = TF2LaunchArgsAction::DropOptionalParams;
	else
		plan.action = TF2LaunchArgsAction::LaunchAsIs;

	return plan;
}

bool tf2_bot_detector::IsSteamLinuxRuntimeSniperUsable(const std::filesystem::path& runPath)
{
	std::error_code ec;
	if (!std::filesystem::is_regular_file(runPath, ec) || ec)
		return false;

#ifdef _WIN32
	return true;
#else
	return ::access(runPath.c_str(), X_OK) == 0;
#endif
}

void tf2_bot_detector::DeleteOldFiles(const std::filesystem::path& path, duration_t maxAge) try
{
	if (!std::filesystem::exists(path))
		return;

	std::vector<std::filesystem::path> files;
	for (const auto& entry : std::filesystem::directory_iterator(path))
	{
		if (!entry.is_regular_file())
			continue;

		files.push_back(entry.path());
	}

	using file_time_point_t = std::filesystem::file_time_type;
	using file_clock_t = file_time_point_t::clock;
	const auto now = file_clock_t::now();
	for (const auto& path : files)
	{
		const auto lastWriteTime = std::filesystem::last_write_time(path);
		const auto age = now - lastWriteTime;
		if (age > maxAge)
		{
			std::error_code ec;
			DebugLog("Removing old file {}", path);
			std::filesystem::remove(path, ec);
			if (ec)
				LogWarning("Failed to delete {}: {}", path, ec);
		}
	}
}
catch (...)
{
	LogException(MH_SOURCE_LOCATION_CURRENT());
}
