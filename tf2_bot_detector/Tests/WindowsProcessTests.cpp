#ifdef _WIN32

#include "Platform/Platform.h"
#include "Util/PathUtils.h"

#include <catch2/catch_test_macros.hpp>

#include <Windows.h>

#include <chrono>
#include <string>
#include <thread>

using namespace tf2_bot_detector;
using namespace tf2_bot_detector::Platform;

namespace
{
	// Spawns a child that exits on its own after a moment, and lets the test wait for it.
	// Uses the Windows shell's own "timeout" helper rather than shipping a fixture binary.
	struct ScopedChildProcess
	{
		explicit ScopedChildProcess(const wchar_t* commandLine)
		{
			STARTUPINFOW si{};
			si.cb = sizeof(si);
			si.dwFlags = STARTF_USESHOWWINDOW;
			si.wShowWindow = SW_HIDE;

			// CreateProcessW may modify the command line buffer, so it cannot be a literal.
			std::wstring mutableCmdLine(commandLine);

			m_Created = !!CreateProcessW(nullptr, mutableCmdLine.data(), nullptr, nullptr,
				FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &m_Info);
		}

		~ScopedChildProcess()
		{
			if (!m_Created)
				return;

			TerminateProcess(m_Info.hProcess, 0);
			WaitForSingleObject(m_Info.hProcess, 5000);
			CloseHandle(m_Info.hProcess);
			CloseHandle(m_Info.hThread);
		}

		ScopedChildProcess(const ScopedChildProcess&) = delete;
		ScopedChildProcess& operator=(const ScopedChildProcess&) = delete;

		bool Created() const { return m_Created; }
		HANDLE Handle() const { return m_Info.hProcess; }

		bool WaitForExit(DWORD ms) const { return WaitForSingleObject(m_Info.hProcess, ms) == WAIT_OBJECT_0; }

	private:
		bool m_Created = false;
		PROCESS_INFORMATION m_Info{};
	};

	// IsProcessRunning caches a handle per name, so "has it noticed the exit yet" needs
	// polling rather than a single call: the cached-handle branch is what we want to exercise.
	bool PollUntilNotRunning(const char* name, std::chrono::milliseconds budget)
	{
		const auto deadline = std::chrono::steady_clock::now() + budget;
		while (std::chrono::steady_clock::now() < deadline)
		{
			if (!Processes::IsProcessRunning(name))
				return true;

			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}

		return !Processes::IsProcessRunning(name);
	}
}

// The handle cache is keyed by name and consulted before any re-scan, so a stale entry would
// keep reporting a dead process as running -- the Windows-shaped version of the bug that
// kill(pid, 0) caused on Linux, where IsTF2Running() never went false and the RCON client was
// never released.
TEST_CASE("IsProcessRunning - sees a child start and exit", "[tf2bd][windows][process]")
{
	// cmd.exe is always present and always running as long as the child lives.
	const ScopedChildProcess child(L"cmd.exe /c timeout /t 30 /nobreak");
	REQUIRE(child.Created());

	// Give it a moment to appear in the process list.
	bool appeared = false;
	for (int i = 0; i < 200 && !appeared; ++i)
	{
		appeared = Processes::IsProcessRunning("cmd.exe");
		if (!appeared)
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}

	// If cmd.exe was already running for unrelated reasons this proves less than we would
	// like, but it must never report false while our own child is definitely alive.
	REQUIRE(appeared);

	// Populate the handle cache, then kill the process behind its back.
	CHECK(Processes::IsProcessRunning("cmd.exe"));

	TerminateProcess(child.Handle(), 0);
	REQUIRE(child.WaitForExit(5000));

	// Only meaningful if nothing else on the machine is running cmd.exe. Skip rather than
	// flake when a developer has a shell open.
	if (Processes::IsProcessRunning("cmd.exe"))
	{
		WARN("another cmd.exe is running; cannot assert the exit is observed");
		return;
	}

	CHECK(PollUntilNotRunning("cmd.exe", std::chrono::milliseconds(2000)));
}

TEST_CASE("IsProcessRunning - a name that cannot exist is not running", "[tf2bd][windows][process]")
{
	CHECK_FALSE(Processes::IsProcessRunning("tf2bd_this_process_does_not_exist.exe"));

	// Repeat: the miss path must not populate the cache with a null handle, which previously
	// would have been stored unchecked and then closed on the next call.
	CHECK_FALSE(Processes::IsProcessRunning("tf2bd_this_process_does_not_exist.exe"));
}

// The old implementation keyed the cache on string_view::data(), which is not guaranteed to be
// null-terminated. Every caller happened to pass a literal, so it never bit -- but IsTF2Running
// now feeds it TF2ProcessNames(), so pass a deliberately non-terminated view.
TEST_CASE("IsProcessRunning - handles a non-null-terminated string_view", "[tf2bd][windows][process]")
{
	const std::string backing = "cmd.exeAAAAAAAA";
	const std::string_view justTheName(backing.data(), 7);
	REQUIRE(justTheName == "cmd.exe");

	// The assertion is that this agrees with the terminated form, not what the answer is:
	// reading past the view would look for "cmd.exeAAAAAAAA" and always return false.
	CHECK(Processes::IsProcessRunning(justTheName) == Processes::IsProcessRunning("cmd.exe"));
}

// IsTF2Running used to be FindWindowA("Valve001", nullptr) -- a window-class lookup shared by
// every Source game. It is now a process scan, so it must agree with scanning the same list by
// hand. With TF2 closed both are false; with TF2 open both are true. Either way they match.
TEST_CASE("IsTF2Running - agrees with a manual scan of TF2ProcessNames", "[tf2bd][windows][process]")
{
	bool anyByName = false;
	for (const auto name : TF2ProcessNames())
	{
		if (Processes::IsProcessRunning(name))
		{
			anyByName = true;
			break;
		}
	}

	CHECK(Processes::IsTF2Running() == anyByName);
}

#endif
