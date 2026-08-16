#pragma once

#include "Clock.h"

#include <mh/coroutine/generator.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace tf2_bot_detector
{
	struct [[nodiscard]] DirectoryValidatorResult
	{
		DirectoryValidatorResult(std::filesystem::path path) : m_Path(std::move(path)) {}

		enum class Result
		{
			Valid,

			Empty,            // Path is empty
			DoesNotExist,     // Directory doesn't exist
			NotADirectory,    // Not a directory
			InvalidContents,  // Contents don't match what we expect
			FilesystemError,  // Some sort of std::filesystem::filesystem_error exception
		} m_Result = Result::Valid;

		operator bool() const { return m_Result == Result::Valid; }

		std::filesystem::path m_Path;
		std::string m_Message;
	};

	DirectoryValidatorResult ValidateTFDir(std::filesystem::path path);
	DirectoryValidatorResult ValidateSteamDir(std::filesystem::path path);

	mh::generator<std::filesystem::path> GetSteamLibraryFolders(const std::filesystem::path& steamDir);
	std::filesystem::path FindTFDir(const std::filesystem::path& steamDir);

	// SteamLinuxRuntime_sniper/run across every library in libraryfolders.vdf,
	// or empty if none is present and executable. Symmetric with FindTFDir.
	std::filesystem::path FindSteamLinuxRuntimeSniper(const std::filesystem::path& steamDir);

	// Accepted TF2 game-binary names for the current platform, most-current first.
	// Adding a future name means editing the single array in PathUtils.cpp.
	// Not the same list as TF2ProcessNames() — see the comments on both arrays.
	std::span<const std::string_view> TF2ExecutableNames();

	// Live-process basenames for the current platform. Distinct from
	// TF2ExecutableNames(): on Linux the launch wrapper tf.sh is an on-disk
	// executable but must never be treated as the running game process.
	std::span<const std::string_view> TF2ProcessNames();

	// Find the game binary next to a tf/ directory; nullopt if none present.
	std::optional<std::filesystem::path> FindTF2Executable(const std::filesystem::path& tf_dir);

	// Probe Steam-root candidates (Linux layouts + optional env overrides).
	// Returns the first path that exists (symlinks followed) and contains steamapps/.
	// Empty if none match. Does not invent a Settings override — callers that have
	// Settings::m_SteamDirOverride should apply it themselves.
	std::filesystem::path DiscoverSteamDir(
		const std::filesystem::path& home,
		std::optional<std::filesystem::path> steamRoot = {},
		std::optional<std::filesystem::path> steamBaseFolder = {});

	// Recommended aliases appended when m_UseLaunchRecommendedParams is set.
	// Length is load-bearing: 437 == 512 - this string's size.
	inline constexpr std::string_view kTF2RecommendedLaunchArgs =
		" +alias cl_reload_localization_files"
		" +alias developer"
		" +contimes 0"
		" +alias ip";

	// Observed Source engine command-line length limit (Linux).
	inline constexpr std::size_t kTF2LaunchArgsHardLimit = 512;
	// 437 == 512 - RecommendedParams.length
	inline constexpr std::size_t kTF2LaunchArgsLimitWithRecommended =
		kTF2LaunchArgsHardLimit - kTF2RecommendedLaunchArgs.size();

	static_assert(kTF2LaunchArgsLimitWithRecommended == 437,
		"437 == 512 - RecommendedParams.length; do not 'improve' these numbers");

	enum class TF2LaunchArgsAction
	{
		LaunchAsIs,
		DropOptionalParams,
		Refuse,
	};

	struct TF2LaunchArgsPlan
	{
		TF2LaunchArgsAction action = TF2LaunchArgsAction::LaunchAsIs;
		std::string args;
		std::string error; // populated when action == Refuse
		std::size_t length = 0;
		std::size_t limit = kTF2LaunchArgsHardLimit;
	};

	// Pure decision: always include required args; drop recommended aliases if
	// they would exceed the limit; refuse if the command line still cannot fit.
	TF2LaunchArgsPlan PlanTF2LaunchArgs(
		std::string userArgs,
		std::string_view rconPassword,
		uint16_t rconPort,
		bool useRecommendedParams);

	// True if `run` exists and is executable (SteamLinuxRuntime_sniper/run).
	bool IsSteamLinuxRuntimeSniperUsable(const std::filesystem::path& runPath);

	void DeleteOldFiles(const std::filesystem::path& path, duration_t maxAge);
}
