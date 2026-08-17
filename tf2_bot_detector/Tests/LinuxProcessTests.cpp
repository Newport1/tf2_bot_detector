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

#endif
