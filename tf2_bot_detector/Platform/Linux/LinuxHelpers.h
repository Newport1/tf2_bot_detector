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

    // chatgpt generated code cuz lazy
    inline pid_t getPidFromProcessName(const std::string &processName)
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
                    // Open the cmdline file to read the process name
                    std::filesystem::path cmdlinePath = std::filesystem::current_path().root_directory() / "proc" / entry->d_name / "cmdline";
                    FILE *cmdlineFile = fopen(cmdlinePath.c_str(), "r");
                    if (cmdlineFile != nullptr)
                    {
                        char cmdline[1024];
                        fgets(cmdline, sizeof(cmdline), cmdlineFile);
                        fclose(cmdlineFile);

                        // Compare the process name with the given name
                        if (strstr(cmdline, processName.c_str()) != nullptr)
                        {
                            closedir(dir);
                            return atoi(entry->d_name); // Convert directory name to PID
                        }
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
