// Integration checks against whatever Steam install is actually on this machine.
//
// Hidden by default (the leading '.' in the tag) because the result depends on
// machine state, so `ctest` stays deterministic. Run them deliberately with:
//
//     ./build/tf2_bot_detector/tf2_bot_detector_cli --run-tests "[realsteam]"
//
// These exist because the unit tests use synthetic fixtures, and synthetic
// fixtures are exactly what missed this bug for years: stock v1.7.0 iterated
// only the numeric *attributes* of libraryfolders.vdf, while a current Steam
// writes numbered *child objects* with a "path" key. On a real machine with TF2
// on a non-default library that meant FindTFDir() found nothing at all, and no
// unit test noticed.
//
// Nothing here launches TF2 or writes to the Steam install; it is all reads.

#include "Util/PathUtils.h"
#include "Platform/Platform.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <filesystem>

using namespace tf2_bot_detector;

namespace
{
	// Finding "the Steam install on this machine" is genuinely platform-specific, and
	// not just cosmetically so. Linux keeps it under $HOME in one of several layouts,
	// which DiscoverSteamDir probes. Windows records it in the registry, under
	// HKCU\Software\Valve\Steam, which Platform::GetCurrentSteamDir reads.
	//
	// DiscoverSteamDir knows only the Linux layouts, so calling it on Windows returned
	// empty for any input and skipped every case below -- even on a machine with Steam
	// installed and TF2 on a secondary library, which is precisely what these tests
	// exist to check. DiscoverSteamDir is deliberately left alone: PathUtilsTests
	// asserts it returns empty for synthetic homes, and a registry probe inside it
	// would make those unit tests find the real install instead.
#ifdef _WIN32
	std::filesystem::path RealSteamDir()
	{
		return Platform::GetCurrentSteamDir();
	}
#else
	std::filesystem::path HomeDir()
	{
		if (const char* home = std::getenv("HOME"))
			return home;

		return {};
	}

	std::filesystem::path RealSteamDir()
	{
		const auto home = HomeDir();
		if (home.empty())
			return {};

		return DiscoverSteamDir(home);
	}
#endif
}

TEST_CASE("real install: Steam directory is discoverable", "[.realsteam]")
{
	const auto steamDir = RealSteamDir();
	if (steamDir.empty())
		SKIP("no Steam installation found on this machine");

	INFO("discovered steam dir: " << steamDir);
	REQUIRE(std::filesystem::exists(steamDir / "steamapps"));
}

TEST_CASE("real install: TF2 is found even on a non-default library", "[.realsteam]")
{
	const auto steamDir = RealSteamDir();
	if (steamDir.empty())
		SKIP("no Steam installation found on this machine");

	// The regression this whole work package exists for. If TF2 is installed
	// anywhere Steam knows about, FindTFDir must locate it -- including when it
	// lives on a library other than the default one.
	const auto tfDir = FindTFDir(steamDir);
	if (tfDir.empty())
		SKIP("TF2 does not appear to be installed under this Steam root");

	INFO("found tf dir: " << tfDir);
	CHECK(std::filesystem::exists(tfDir));
	CHECK(ValidateTFDir(tfDir));

	// ...and the unified binary discovery must agree that a game binary is there.
	const auto exe = FindTF2Executable(tfDir);
	REQUIRE(exe.has_value());
	INFO("found tf2 executable: " << *exe);
	CHECK(std::filesystem::exists(*exe));
}

TEST_CASE("real install: library enumeration yields no duplicates", "[.realsteam]")
{
	const auto steamDir = RealSteamDir();
	if (steamDir.empty())
		SKIP("no Steam installation found on this machine");

	// ~/.steam/steam is commonly a symlink (e.g. -> ~/.steam/debian-installation),
	// so a naive childs-aware parse reports the same library twice.
	std::vector<std::filesystem::path> seen;
	for (const auto& lib : GetSteamLibraryFolders(steamDir))
	{
		std::error_code ec;
		auto canonical = std::filesystem::weakly_canonical(lib, ec);
		if (ec)
			canonical = lib;

		INFO("duplicate library yielded: " << canonical);
		CHECK(std::find(seen.begin(), seen.end(), canonical) == seen.end());
		seen.push_back(std::move(canonical));
	}

	REQUIRE_FALSE(seen.empty());
}
