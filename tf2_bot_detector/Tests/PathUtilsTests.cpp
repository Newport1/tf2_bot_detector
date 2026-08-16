#include "Util/PathUtils.h"
#include "Config/Settings.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifndef _WIN32
#include <sys/stat.h>
#endif

using namespace tf2_bot_detector;

namespace
{
	std::filesystem::path SteamFixturesDir()
	{
		return std::filesystem::path(__FILE__).parent_path() / "fixtures" / "steam";
	}

	class TempDir
	{
	public:
		TempDir()
		{
			const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
			static int seq = 0;
			m_Path = std::filesystem::temp_directory_path()
				/ ("tf2bd_t3_" + std::to_string(stamp) + "_" + std::to_string(++seq));
			std::filesystem::create_directories(m_Path);
		}

		~TempDir()
		{
			std::error_code ec;
			std::filesystem::remove_all(m_Path, ec);
		}

		TempDir(const TempDir&) = delete;
		TempDir& operator=(const TempDir&) = delete;

		const std::filesystem::path& path() const { return m_Path; }

	private:
		std::filesystem::path m_Path;
	};

	std::vector<std::filesystem::path> CollectLibraries(const std::filesystem::path& steamDir)
	{
		std::vector<std::filesystem::path> out;
		for (auto folder : GetSteamLibraryFolders(steamDir))
			out.push_back(std::move(folder));
		return out;
	}

	void InstallFixture(const std::filesystem::path& steamDir, const std::string& fixtureName)
	{
		std::filesystem::create_directories(steamDir / "steamapps");
		std::ifstream in(SteamFixturesDir() / fixtureName);
		REQUIRE(in.good());
		std::ofstream out(steamDir / "steamapps" / "libraryfolders.vdf");
		REQUIRE(out.good());
		out << in.rdbuf();
	}

	std::filesystem::path Normalized(const std::filesystem::path& p)
	{
		std::error_code ec;
		auto n = std::filesystem::weakly_canonical(p, ec);
		return ec ? p.lexically_normal() : n;
	}

	void WriteEmptyFile(const std::filesystem::path& path)
	{
		std::filesystem::create_directories(path.parent_path());
		std::ofstream out(path);
		REQUIRE(out.good());
	}

	std::filesystem::path MakeFakeTFDir(const std::filesystem::path& root, std::string_view binaryName)
	{
		const auto tfDir = root / "tf";
		std::filesystem::create_directories(tfDir / "custom");
		WriteEmptyFile(tfDir / "tf2_misc_dir.vpk");
		WriteEmptyFile(tfDir / "tf2_sound_misc_dir.vpk");
		WriteEmptyFile(tfDir / "tf2_textures_dir.vpk");
		if (!binaryName.empty())
			WriteEmptyFile(root / binaryName);
		return tfDir;
	}
}

TEST_CASE("GetSteamLibraryFolders - default library only", "[tf2bd][pathutils]")
{
	TempDir tmp;
	InstallFixture(tmp.path(), "default_only.vdf");

	const auto folders = CollectLibraries(tmp.path());
	REQUIRE(folders.size() == 1);
	REQUIRE(folders.front() == Normalized(tmp.path() / "steamapps"));
}

TEST_CASE("GetSteamLibraryFolders - legacy numeric-attribute format", "[tf2bd][pathutils]")
{
	TempDir tmp;
	InstallFixture(tmp.path(), "legacy.vdf");

	const auto folders = CollectLibraries(tmp.path());
	REQUIRE(folders.size() == 2);
	REQUIRE(folders[0] == Normalized(tmp.path() / "steamapps"));
	REQUIRE(folders[1] == Normalized(std::filesystem::path("/mnt/tf2bd-test-second-drive/SteamLibrary") / "steamapps"));
}

TEST_CASE("GetSteamLibraryFolders - modern numeric-child-object format with path", "[tf2bd][pathutils]")
{
	TempDir tmp;
	InstallFixture(tmp.path(), "modern.vdf");

	const auto folders = CollectLibraries(tmp.path());
	REQUIRE_FALSE(folders.empty());
	REQUIRE(folders[0] == Normalized(tmp.path() / "steamapps"));

	const auto modern = Normalized(std::filesystem::path("/tmp/tf2bd-test-default-steam") / "steamapps");
	REQUIRE(std::find(folders.begin(), folders.end(), modern) != folders.end());
}

TEST_CASE("GetSteamLibraryFolders - second drive path is correct", "[tf2bd][pathutils]")
{
	TempDir tmp;
	InstallFixture(tmp.path(), "modern_second_drive.vdf");

	const auto folders = CollectLibraries(tmp.path());
	const auto second = Normalized(std::filesystem::path("/mnt/tf2bd-test-second-drive/SteamLibrary") / "steamapps");
	REQUIRE(std::find(folders.begin(), folders.end(), second) != folders.end());
}

TEST_CASE("GetSteamLibraryFolders - numeric child missing path is skipped", "[tf2bd][pathutils]")
{
	TempDir tmp;
	InstallFixture(tmp.path(), "modern_missing_path.vdf");

	std::vector<std::filesystem::path> folders;
	REQUIRE_NOTHROW(folders = CollectLibraries(tmp.path()));

	const auto modernDefault = Normalized(std::filesystem::path("/tmp/tf2bd-test-default-steam") / "steamapps");
	REQUIRE(std::find(folders.begin(), folders.end(), modernDefault) != folders.end());
	// The child with no path key must not invent a steamapps entry.
	for (const auto& folder : folders)
		REQUIRE(folder.filename() == "steamapps");
}

TEST_CASE("GetSteamLibraryFolders - same library in both formats yielded once", "[tf2bd][pathutils]")
{
	TempDir tmp;
	InstallFixture(tmp.path(), "both_formats.vdf");

	const auto folders = CollectLibraries(tmp.path());
	const auto second = Normalized(std::filesystem::path("/mnt/tf2bd-test-second-drive/SteamLibrary") / "steamapps");

	const auto count = std::count(folders.begin(), folders.end(), second);
	REQUIRE(count == 1);
	REQUIRE(folders.size() == 2);
}

TEST_CASE("GetSteamLibraryFolders - malformed VDF does not throw or invent paths", "[tf2bd][pathutils]")
{
	TempDir tmp;
	InstallFixture(tmp.path(), "malformed.vdf");

	std::vector<std::filesystem::path> folders;
	REQUIRE_NOTHROW(folders = CollectLibraries(tmp.path()));
	REQUIRE(folders.size() == 1);
	REQUIRE(folders.front() == Normalized(tmp.path() / "steamapps"));

	const auto bogus = Normalized(std::filesystem::path("/mnt/tf2bd-test-should-not-appear") / "steamapps");
	REQUIRE(std::find(folders.begin(), folders.end(), bogus) == folders.end());
}

TEST_CASE("FindTF2Executable / ValidateTFDir - accepted names", "[tf2bd][pathutils]")
{
	const auto names = TF2ExecutableNames();
	REQUIRE_FALSE(names.empty());

	for (const auto name : names)
	{
		TempDir tmp;
		const auto tfDir = MakeFakeTFDir(tmp.path(), name);

		const auto found = FindTF2Executable(tfDir);
		REQUIRE(found.has_value());
		REQUIRE(found->filename() == std::filesystem::path(name));

		const auto valid = ValidateTFDir(tfDir);
		REQUIRE(valid);
	}
}

TEST_CASE("FindTF2Executable / ValidateTFDir - unknown binary is rejected", "[tf2bd][pathutils]")
{
	TempDir tmp;
	const auto tfDir = MakeFakeTFDir(tmp.path(), "not_a_tf2_binary");

	REQUIRE_FALSE(FindTF2Executable(tfDir).has_value());
	REQUIRE_FALSE(ValidateTFDir(tfDir));
}

TEST_CASE("FindTF2Executable / ValidateTFDir - tf/ with no sibling binary", "[tf2bd][pathutils]")
{
	TempDir tmp;
	const auto tfDir = MakeFakeTFDir(tmp.path(), "");

	REQUIRE_FALSE(FindTF2Executable(tfDir).has_value());
	REQUIRE_FALSE(ValidateTFDir(tfDir));
}

TEST_CASE("FindTF2Executable / ValidateTFDir / Settings agree", "[tf2bd][pathutils]")
{
	const auto names = TF2ExecutableNames();

	GeneralSettings settings;
	const TFBinaryMode modes[] = { TFBinaryMode::x64, TFBinaryMode::x86, TFBinaryMode::x86_legacy };
	for (const auto mode : modes)
	{
		settings.m_TFBinaryMode = mode;
		const auto chosen = settings.GetBinaryName();
		const bool inList = std::any_of(names.begin(), names.end(),
			[&](std::string_view n) { return n == chosen; });
		REQUIRE(inList);
	}

	// Every name the validator accepts is the same set Settings can pick from.
	for (const auto name : names)
	{
		TempDir tmp;
		const auto tfDir = MakeFakeTFDir(tmp.path(), name);
		REQUIRE(FindTF2Executable(tfDir).has_value());
		REQUIRE(ValidateTFDir(tfDir));
	}
}

TEST_CASE("DiscoverSteamDir - probe order and steamapps requirement", "[tf2bd][pathutils]")
{
	TempDir home;

	const auto classic = home.path() / ".steam" / "steam";
	const auto xdg = home.path() / ".local" / "share" / "Steam";
	const auto flatpak = home.path() / ".var" / "app" / "com.valvesoftware.Steam" / ".local" / "share" / "Steam";
	const auto rootLink = home.path() / ".steam" / "root";
	const auto envRoot = home.path() / "env-steam-root";
	const auto envBase = home.path() / "env-steam-base";

	// Nothing exists yet.
	REQUIRE(DiscoverSteamDir(home.path(), envRoot, envBase).empty());

	// A candidate without steamapps is ignored.
	std::filesystem::create_directories(classic);
	REQUIRE(DiscoverSteamDir(home.path()).empty());

	std::filesystem::create_directories(xdg / "steamapps");
	std::filesystem::create_directories(classic / "steamapps");

	// ~/.steam/steam beats ~/.local/share/Steam when both are valid.
	REQUIRE(Normalized(DiscoverSteamDir(home.path())) == Normalized(classic));

	// $STEAM_ROOT wins over the well-known paths.
	std::filesystem::create_directories(envRoot / "steamapps");
	REQUIRE(Normalized(DiscoverSteamDir(home.path(), envRoot, {})) == Normalized(envRoot));

	// $STEAM_BASE_FOLDER is used when STEAM_ROOT is unset.
	std::filesystem::create_directories(envBase / "steamapps");
	REQUIRE(Normalized(DiscoverSteamDir(home.path(), {}, envBase)) == Normalized(envBase));

	// Flatpak / .steam/root are reachable when earlier candidates are absent.
	TempDir home2;
	std::filesystem::create_directories(home2.path() / ".var" / "app" / "com.valvesoftware.Steam" / ".local" / "share" / "Steam" / "steamapps");
	REQUIRE(Normalized(DiscoverSteamDir(home2.path())) ==
		Normalized(home2.path() / ".var" / "app" / "com.valvesoftware.Steam" / ".local" / "share" / "Steam"));

	TempDir home3;
	std::filesystem::create_directories(home3.path() / ".steam" / "root" / "steamapps");
	REQUIRE(Normalized(DiscoverSteamDir(home3.path())) == Normalized(home3.path() / ".steam" / "root"));

	(void)flatpak;
	(void)rootLink;
}

TEST_CASE("PlanTF2LaunchArgs - under the limit launches as-is", "[tf2bd][pathutils]")
{
	const auto plan = PlanTF2LaunchArgs("-novid", "pw", 40000, true);
	REQUIRE(plan.action == TF2LaunchArgsAction::LaunchAsIs);
	REQUIRE(plan.length <= kTF2LaunchArgsHardLimit);
	REQUIRE(plan.args.find("cl_reload_localization_files") != std::string::npos);
	REQUIRE(plan.args.find("-novid") != std::string::npos);
	REQUIRE(plan.args.find("+rcon_password pw") != std::string::npos);
}

TEST_CASE("PlanTF2LaunchArgs - over 437 drops recommended then launches", "[tf2bd][pathutils]")
{
	const auto baseline = PlanTF2LaunchArgs("", "pw", 40000, false);
	REQUIRE(baseline.length < kTF2LaunchArgsLimitWithRecommended);

	const auto pad = kTF2LaunchArgsLimitWithRecommended + 1 - baseline.length;
	const std::string user(pad, 'x');

	const auto plan = PlanTF2LaunchArgs(user, "pw", 40000, true);
	REQUIRE(plan.action == TF2LaunchArgsAction::DropOptionalParams);
	REQUIRE(plan.length <= kTF2LaunchArgsHardLimit);
	REQUIRE(plan.args.find("cl_reload_localization_files") == std::string::npos);
	REQUIRE(plan.args.find("+rcon_password pw") != std::string::npos);
}

TEST_CASE("PlanTF2LaunchArgs - over 512 after drop refuses with length and limit", "[tf2bd][pathutils]")
{
	const auto baseline = PlanTF2LaunchArgs("", "pw", 40000, false);
	const auto pad = kTF2LaunchArgsHardLimit + 1 - baseline.length;
	const std::string user(pad, 'x');

	const auto plan = PlanTF2LaunchArgs(user, "pw", 40000, true);
	REQUIRE(plan.action == TF2LaunchArgsAction::Refuse);
	REQUIRE(plan.length > kTF2LaunchArgsHardLimit);
	REQUIRE(plan.error.find(std::to_string(plan.length)) != std::string::npos);
	REQUIRE(plan.error.find(std::to_string(kTF2LaunchArgsHardLimit)) != std::string::npos);
	REQUIRE(plan.error.find(user) != std::string::npos);
}

TEST_CASE("IsSteamLinuxRuntimeSniperUsable", "[tf2bd][pathutils]")
{
	TempDir tmp;
	const auto missing = tmp.path() / "SteamLinuxRuntime_sniper" / "run";
	REQUIRE_FALSE(IsSteamLinuxRuntimeSniperUsable(missing));

	WriteEmptyFile(missing);
#ifndef _WIN32
	REQUIRE_FALSE(IsSteamLinuxRuntimeSniperUsable(missing)); // not executable
	REQUIRE(::chmod(missing.c_str(), 0755) == 0);
#endif
	REQUIRE(IsSteamLinuxRuntimeSniperUsable(missing));
}

static void WriteLibraryFoldersVdf(const std::filesystem::path& steamDir,
	const std::filesystem::path& defaultLib,
	const std::filesystem::path& secondLib)
{
	std::filesystem::create_directories(steamDir / "steamapps");
	std::ofstream out(steamDir / "steamapps" / "libraryfolders.vdf");
	REQUIRE(out.good());
	out << "\"libraryfolders\"\n{\n"
		<< "\t\"0\"\n\t{\n\t\t\"path\"\t\t\"" << defaultLib.generic_string() << "\"\n\t}\n"
		<< "\t\"1\"\n\t{\n\t\t\"path\"\t\t\"" << secondLib.generic_string() << "\"\n\t}\n"
		<< "}\n";
}

TEST_CASE("FindSteamLinuxRuntimeSniper - second library only", "[tf2bd][pathutils]")
{
	TempDir tmp;
	const auto steamDir = tmp.path() / "steam";
	const auto secondLib = tmp.path() / "second_library";
	WriteLibraryFoldersVdf(steamDir, steamDir, secondLib);

	const auto runPath = secondLib / "steamapps" / "common" / "SteamLinuxRuntime_sniper" / "run";
	WriteEmptyFile(runPath);
#ifndef _WIN32
	REQUIRE(::chmod(runPath.c_str(), 0755) == 0);
#endif

	// Default library has steamapps but no sniper; only the second library does.
	const auto found = FindSteamLinuxRuntimeSniper(steamDir);
	REQUIRE_FALSE(found.empty());
	REQUIRE(Normalized(found) == Normalized(runPath));
}

TEST_CASE("FindSteamLinuxRuntimeSniper - absent everywhere returns empty", "[tf2bd][pathutils]")
{
	TempDir tmp;
	const auto steamDir = tmp.path() / "steam";
	const auto secondLib = tmp.path() / "second_library";
	WriteLibraryFoldersVdf(steamDir, steamDir, secondLib);
	std::filesystem::create_directories(secondLib / "steamapps" / "common");

	REQUIRE(FindSteamLinuxRuntimeSniper(steamDir).empty());
}

TEST_CASE("TF2ProcessNames is not TF2ExecutableNames", "[tf2bd][pathutils]")
{
	const auto procs = TF2ProcessNames();
	REQUIRE_FALSE(procs.empty());

	const bool hasTfSh = std::any_of(procs.begin(), procs.end(),
		[](std::string_view n) { return n == "tf.sh"; });
	REQUIRE_FALSE(hasTfSh);

#ifdef _WIN32
	const auto exes = TF2ExecutableNames();
	REQUIRE(procs.size() == exes.size());
	REQUIRE(std::equal(procs.begin(), procs.end(), exes.begin()));
#endif
}

// safety-boundary-allow: asserts TF2BD does NOT override a user who asked for
// insecure. The flag appears here only as test input, never as something we emit.
TEST_CASE("PlanTF2LaunchArgs - asks for -secure by default", "[tf2bd][pathutils]")
{
	const auto plan = tf2_bot_detector::PlanTF2LaunchArgs("-novid", "pw", 40434, false);

	REQUIRE(plan.action == tf2_bot_detector::TF2LaunchArgsAction::LaunchAsIs);
	REQUIRE(plan.args.find("-secure") != std::string::npos);
}

// safety-boundary-allow: see above.
TEST_CASE("PlanTF2LaunchArgs - user's -insecure is not overridden", "[tf2bd][pathutils]") // safety-boundary-allow
{
	const auto plan = tf2_bot_detector::PlanTF2LaunchArgs("-novid -insecure", "pw", 40434, false); // safety-boundary-allow

	REQUIRE(plan.action == tf2_bot_detector::TF2LaunchArgsAction::LaunchAsIs);
	// The user's own flag survives, and we must not append a contradicting -secure.
	REQUIRE(plan.args.find("-insecure") != std::string::npos); // safety-boundary-allow
	REQUIRE(plan.args.find("-secure") == std::string::npos);
	// Everything else TF2BD needs is still there.
	REQUIRE(plan.args.find("-usercon") != std::string::npos);
	REQUIRE(plan.args.find("-condebug") != std::string::npos);
}

// safety-boundary-allow: see above.
TEST_CASE("PlanTF2LaunchArgs - substring is not mistaken for the flag", "[tf2bd][pathutils]")
{
	// "-insecurely" is not "-insecure", so -secure must still be requested. // safety-boundary-allow
	const auto plan = tf2_bot_detector::PlanTF2LaunchArgs("-insecurely", "pw", 40434, false); // safety-boundary-allow

	REQUIRE(plan.args.find("-secure") != std::string::npos);
}
