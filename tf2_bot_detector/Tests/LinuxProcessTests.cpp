#ifndef _WIN32

#include "Platform/Linux/LinuxHelpers.h"

#include <catch2/catch_test_macros.hpp>

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>

using namespace tf2_bot_detector;

namespace
{
	struct ProcessFixture
	{
		pid_t wrapper = -1;
		pid_t target = -1;
		int holdFd = -1;
		std::filesystem::path copiedExe;

		~ProcessFixture()
		{
			if (holdFd >= 0)
				close(holdFd);

			for (const pid_t pid : { wrapper, target })
			{
				if (pid <= 0)
					continue;

				kill(pid, SIGKILL);
				int status = 0;
				waitpid(pid, &status, 0);
			}

			std::error_code ec;
			if (!copiedExe.empty())
				std::filesystem::remove(copiedExe, ec);
		}
	};

	std::string ReadProcCmdline(pid_t pid)
	{
		std::ifstream file(std::filesystem::path("/proc") / std::to_string(pid) / "cmdline", std::ios::binary);
		return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
	}

	bool ExeBasenameIs(pid_t pid, const std::string& name)
	{
		std::error_code ec;
		const auto exe = std::filesystem::read_symlink(
			std::filesystem::path("/proc") / std::to_string(pid) / "exe", ec);
		return !ec && exe.filename() == name;
	}

	// Exec is asynchronous. A fixed sleep both flakes and, when the child has
	// already exited, observes an empty /proc/<pid>/cmdline.
	template <typename Pred>
	bool PollUntil(Pred&& pred)
	{
		for (int i = 0; i < 200 && !pred(); ++i)
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		return pred();
	}
}

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

// The old scan was strstr() over /proc/<pid>/cmdline. fgets() keeps embedded
// NULs, but strstr() stops at the first one, so a wrapper whose argv0 merely
// contains "tf_linux64" was treated as the game. GNU sleep rejects
// "--wrapper-target=..." and exits, leaving a zombie with an empty cmdline, so
// that assertion passed without the wrapper still being visible. sh stays
// blocked in `read` with the marker on its cmdline until we close the pipe.
TEST_CASE("getPidFromProcessName - exact executable name matching", "[tf2bd][linux][process]")
{
	constexpr const char* kWrapperMarker = "--wrapper-target=tf_linux64";

	int pipefd[2] = { -1, -1 };
	REQUIRE(pipe(pipefd) == 0);

	ProcessFixture procs;
	procs.holdFd = pipefd[1];
	procs.wrapper = fork();
	if (procs.wrapper == 0)
	{
		close(pipefd[1]);
		if (dup2(pipefd[0], STDIN_FILENO) < 0)
			_exit(1);
		close(pipefd[0]);

		// argv0 contains "tf_linux64" so the old strstr() scan (which stops at
		// the first NUL) matches this wrapper. A leading '-' would make sh a
		// login shell, so the marker flag itself is a later argument. `read`
		// is a shell builtin and blocks while the pipe's write end stays open.
		execlp("sh", "tf_linux64-wrapper", "-c", "read _", "sh", kWrapperMarker, nullptr);
		_exit(1);
	}
	close(pipefd[0]);
	REQUIRE(procs.wrapper != -1);
	// Drop the write end on exec so the copied sleep does not keep `read` alive
	// after this fixture closes holdFd.
	REQUIRE(fcntl(procs.holdFd, F_SETFD, FD_CLOEXEC) != -1);

	const std::string sleepSrc = std::filesystem::exists("/bin/sleep") ? "/bin/sleep" : "/usr/bin/sleep";
	const std::string uniqueTargetName = "tf2bd_test_target_" + std::to_string(getpid());
	procs.copiedExe = std::filesystem::temp_directory_path() / uniqueTargetName;

	std::error_code ec;
	std::filesystem::copy_file(sleepSrc, procs.copiedExe, std::filesystem::copy_options::overwrite_existing, ec);
	REQUIRE_FALSE(ec);
	std::filesystem::permissions(procs.copiedExe, std::filesystem::perms::owner_all, ec);

	procs.target = fork();
	if (procs.target == 0)
	{
		execl(procs.copiedExe.c_str(), procs.copiedExe.c_str(), "60", nullptr);
		_exit(1);
	}
	REQUIRE(procs.target != -1);

	INFO("wrapper cmdline was not observable");
	REQUIRE(PollUntil([&] {
		return Linux::IsProcessRunningPid(procs.wrapper)
			&& ReadProcCmdline(procs.wrapper).find(kWrapperMarker) != std::string::npos;
	}));

	INFO("target executable was not observable");
	REQUIRE(PollUntil([&] {
		return Linux::IsProcessRunningPid(procs.target)
			&& ExeBasenameIs(procs.target, uniqueTargetName);
	}));

	const pid_t foundGame = Linux::getPidFromProcessName("tf_linux64");
	CHECK(foundGame != procs.wrapper);
	if (foundGame != -1)
		CHECK(ExeBasenameIs(foundGame, "tf_linux64"));

	CHECK(Linux::getPidFromProcessName(uniqueTargetName) == procs.target);
}

#endif
