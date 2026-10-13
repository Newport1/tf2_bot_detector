#ifndef _WIN32

#include "Platform/Linux/LinuxHelpers.h"

#include <catch2/catch_test_macros.hpp>

#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <thread>

using namespace tf2_bot_detector;

// The bug this guards against: kill(pid, 0) succeeds for a zombie, so quitting TF2
// left IsTF2Running() true forever -- the RCON client was never released and the UI
// never returned to the launch button. Fork a real zombie and assert we see through it.
TEST_CASE("IsProcessRunningPid - a zombie is not running", "[tf2bd][linux][process]")
{
	const pid_t child = fork();
	REQUIRE(child != -1);

	if (child == 0)
		_exit(0); // becomes <defunct>: we deliberately do not wait() yet

	// Give the child a moment to actually die. Poll rather than sleep a fixed amount,
	// so this stays fast and does not flake on a loaded machine.
	bool reachedZombie = false;
	for (int i = 0; i < 200 && !reachedZombie; ++i)
	{
		std::ifstream stat(std::filesystem::path("/proc") / std::to_string(child) / "stat");
		std::string line;
		if (std::getline(stat, line))
		{
			const auto commEnd = line.rfind(')');
			reachedZombie = commEnd != std::string::npos
				&& (commEnd + 2) < line.size()
				&& line[commEnd + 2] == 'Z';
		}

		if (!reachedZombie)
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}

	REQUIRE(reachedZombie); // if this fails the test proved nothing, so fail loudly
	CHECK_FALSE(Linux::IsProcessRunningPid(child));

	int status = 0;
	waitpid(child, &status, 0); // reap, so we do not leak a zombie into the test run

	// Reaped: the /proc entry is gone and it must still report not-running.
	CHECK_FALSE(Linux::IsProcessRunningPid(child));
}

TEST_CASE("IsProcessRunningPid - self is running, -1 is not", "[tf2bd][linux][process]")
{
	CHECK(Linux::IsProcessRunningPid(getpid()));
	CHECK_FALSE(Linux::IsProcessRunningPid(-1));
}

// getPidFromProcessName must match the executable basename (argv[0] of
// /proc/<pid>/cmdline) exactly against TF2ProcessNames(), not substring-match
// the whole cmdline. The old strstr() check matched any argv merely mentioning
// a game binary name -- including the tf.sh launch wrapper -- and
// GetTF2CommandLineArgsAsync could then latch onto the wrapper pid instead of
// the game. Spawn a fake "tf_linux64" (a symlink to sleep, so argv[0] is a
// full path and only its basename matches) plus a decoy whose argv merely
// mentions tf_linux64, and assert the lookup finds the former but not the
// latter.
TEST_CASE("getPidFromProcessName - matches executable basename against TF2ProcessNames", "[tf2bd][linux][process]")
{
	REQUIRE(!TF2ProcessNames().empty());
	const std::string gameName(TF2ProcessNames().front());

	// Prefer /bin/sleep, fall back to /usr/bin/sleep.
	std::filesystem::path sleeper("/bin/sleep");
	if (!std::filesystem::exists(sleeper))
		sleeper = "/usr/bin/sleep";
	REQUIRE(std::filesystem::exists(sleeper));

	// The fake game binary: a symlink so argv[0] is a full path and the
	// matcher has to take the basename to find it.
	const std::filesystem::path tmpDir =
		std::filesystem::temp_directory_path() / ("tf2bd-pid-test-" + std::to_string(getpid()));
	std::filesystem::create_directories(tmpDir);
	const std::filesystem::path fakeGame = tmpDir / gameName;
	std::error_code ec;
	std::filesystem::create_symlink(sleeper, fakeGame, ec);
	REQUIRE(!ec);

	// Decoy first: its cmdline mentions the game name, but its executable
	// basename ("sh") is not a TF2 process name, so it must never match.
	const pid_t decoy = fork();
	REQUIRE(decoy != -1);
	if (decoy == 0)
	{
		execl("/bin/sh", "sh", "-c", "sleep 120", gameName.c_str(), nullptr);
		_exit(127); // execl failed
	}

	// Give the decoy a moment to exec so its /proc cmdline is settled. Even
	// pre-exec this holds: the forked test binary's own basename is not a
	// TF2 process name either.
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	CHECK(Linux::getPidFromProcessName() != decoy);

	const pid_t game = fork();
	REQUIRE(game != -1);
	if (game == 0)
	{
		execl(fakeGame.c_str(), fakeGame.c_str(), "120", nullptr);
		_exit(127); // execl failed
	}

	// Poll rather than sleep a fixed amount, so this stays fast and does not
	// flake on a loaded machine.
	pid_t found = -1;
	for (int i = 0; i < 200 && found != game; ++i)
	{
		found = Linux::getPidFromProcessName();
		if (found != game)
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	CHECK(found == game);

	kill(decoy, SIGKILL);
	kill(game, SIGKILL);
	int status = 0;
	waitpid(decoy, &status, 0);
	waitpid(game, &status, 0);

	std::error_code rmEc;
	std::filesystem::remove_all(tmpDir, rmEc);
}

#endif
