#pragma once

#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <filesystem>

#include "Util/PathUtils.h"

#include <unistd.h>
#include <dirent.h>
#include <signal.h>

#include <fstream>
#include <string>

namespace tf2_bot_detector::Linux
{
    // PID cache
    inline std::unordered_map<std::string_view, pid_t> processPids;

    // Find the PID of a running TF2 game process.
    //
    // Matches the executable basename (argv[0] of /proc/<pid>/cmdline) exactly
    // against TF2ProcessNames(), instead of substring-matching the whole
    // cmdline. The old strstr() check had false positives: the tf.sh launch
    // wrapper, and any process whose argv merely mentioned a game binary name,
    // would match -- and GetTF2CommandLineArgsAsync could then latch onto the
    // wrapper pid instead of the game. See the kTF2ProcessNames comment in
    // PathUtils.cpp.
    inline pid_t getPidFromProcessName()
    {
        DIR *dir = opendir("/proc");
        if (dir != nullptr)
        {
            struct dirent *entry;
            while ((entry = readdir(dir)) != nullptr)
            {
                // Check if the entry is a directory and its name is a number (PID)
                if (entry->d_type == DT_DIR && std::isdigit(entry->d_name[0]))
                {
                    // argv[0] of /proc/<pid>/cmdline is the executable the process
                    // was started with; its basename is what TF2ProcessNames()
                    // lists (e.g. tf_linux64), never the tf.sh wrapper.
                    const std::filesystem::path cmdlinePath =
                        std::filesystem::path("/proc") / entry->d_name / "cmdline";
                    std::ifstream cmdlineFile(cmdlinePath, std::ios::binary);
                    std::string argv0;
                    if (cmdlineFile && std::getline(cmdlineFile, argv0, '\0'))
                    {
                        const std::string basename =
                            std::filesystem::path(argv0).filename().string();

                        for (const auto name : TF2ProcessNames())
                        {
                            if (basename == name)
                            {
                                closedir(dir);
                                return atoi(entry->d_name); // Convert directory name to PID
                            }
                        }
                    }
                }
            }
            closedir(dir);
        }
        return -1; // Return -1 if no TF2 process was found
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
