#pragma once

#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <filesystem>

#include <unistd.h>
#include <dirent.h>
#include <signal.h>

#include <fstream>
#include <string>

namespace tf2_bot_detector::Linux
{
    // PID cache
    inline std::unordered_map<std::string_view, pid_t> processPids;

    inline pid_t getPidFromProcessName(const std::string &processName)
    {
        const pid_t selfPid = ::getpid();
        DIR *dir = opendir("/proc");
        if (dir != nullptr)
        {
            struct dirent *entry;
            while ((entry = readdir(dir)) != nullptr)
            {
                // Check if the entry is a directory and its name is a number (PID)
                if (entry->d_type == DT_DIR && std::isdigit(entry->d_name[0]))
                {
                    const pid_t pid = atoi(entry->d_name);
                    if (pid == selfPid)
                        continue;

                    std::string matchedName;
                    std::error_code ec;
                    std::filesystem::path exePath = std::filesystem::read_symlink(
                        std::filesystem::path("/proc") / entry->d_name / "exe", ec);

                    if (!ec && !exePath.empty())
                    {
                        matchedName = exePath.filename().string();
                    }
                    else
                    {
                        std::ifstream cmdlineFile(std::filesystem::path("/proc") / entry->d_name / "cmdline", std::ios::binary);
                        if (cmdlineFile.is_open())
                        {
                            std::string argv0;
                            std::getline(cmdlineFile, argv0, '\0');
                            if (!argv0.empty())
                            {
                                matchedName = std::filesystem::path(argv0).filename().string();
                            }
                        }
                    }

                    if (matchedName == processName)
                    {
                        closedir(dir);
                        return pid;
                    }
                }
            }
            closedir(dir);
        }
        return -1; // Return -1 if the process name is not found
    }

    // if you dont have ~/.steam/steam.pid, this will fail
    // but i dont really understand why you won't have .steam/steam.pid
    // TODO if /.steam/steam.pid fails, grab it from getPidFromProcessName()
    inline pid_t GetSteamPID()
    {
        std::filesystem::path steam_pid_path = std::filesystem::path(getenv("HOME")) / ".steam" / "steam.pid";
        std::ifstream steam_pid_file(steam_pid_path);

        pid_t ret;
        steam_pid_file >> ret;
        return ret;
    }

    inline bool IsProcessRunningPid(pid_t pid)
    {
        if (pid == -1)
        {
            return false;
        }

        // NOT kill(pid, 0). That succeeds for a zombie -- a process that has already
        // exited but whose parent has not reaped it yet. TF2 routinely lingers as
        // <defunct> after you quit (which is also why Steam keeps offering "Quit
        // Game"), so kill() reported the game as alive indefinitely: IsTF2Running()
        // never went false, the RCON client was never released, and the UI stayed
        // stuck on an empty player list with no way back to the launch button.
        //
        // The state field of /proc/<pid>/stat is what actually tells them apart.
        std::ifstream stat(std::filesystem::path("/proc") / std::to_string(pid) / "stat");

        std::string line;
        if (!std::getline(stat, line))
        {
            return false; // no /proc entry at all: really gone
        }

        // Field 2 (comm) is parenthesised and may itself contain ')' and spaces, so
        // anchor on the LAST ')' rather than tokenising from the left.
        const auto commEnd = line.rfind(')');
        if (commEnd == std::string::npos || (commEnd + 2) >= line.size())
        {
            return false;
        }

        return line[commEnd + 2] != 'Z';
    }
}
