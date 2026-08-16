#include "../Platform.h"
#include "Util/TextUtils.h"
#include "Log.h"

#include <mh/text/string_insertion.hpp>
#include <fmt/format.h>
#include <fmt/std.h>
#include <fmt/ostream.h>
#include <fmt/chrono.h>
#include <fmt/xchar.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>


/// @brief opens anything with xdg-open
/// @warning potentially, i mean very insecure. do not put any user data in this function.
/// @param url 
void xdg_open(const char* url)
{
    system(fmt::format("xdg-open {}", url).c_str());
}

// Unix-style tokenizer. string_view::data() is not NUL-terminated, so we copy first.
// Windows SplitCommandLineArgs correctly defers to CommandLineToArgvW and is left alone.
std::vector<std::string> tf2_bot_detector::Shell::SplitCommandLineArgs(const std::string_view& cmdline)
{
	const std::string input(cmdline);
	std::vector<std::string> tokens;
	std::string current;

	enum class State { Normal, InDouble, InSingle };
	State state = State::Normal;
	bool inToken = false;

	const auto isSeparator = [](char c) { return c == ' ' || c == '\t'; };

	for (size_t i = 0; i < input.size(); ++i)
	{
		const char c = input[i];

		if (state == State::Normal)
		{
			if (c == '\\' && i + 1 < input.size())
			{
				const char next = input[i + 1];
				if (next == '"' || next == '\'' || next == '\\' || isSeparator(next))
				{
					current.push_back(next);
					inToken = true;
					++i;
					continue;
				}
			}

			if (c == '"')
			{
				state = State::InDouble;
				inToken = true; // "" is one empty token
				continue;
			}
			if (c == '\'')
			{
				state = State::InSingle;
				inToken = true;
				continue;
			}
			if (isSeparator(c))
			{
				if (inToken)
				{
					tokens.push_back(std::move(current));
					current.clear();
					inToken = false;
				}
				continue;
			}

			current.push_back(c);
			inToken = true;
			continue;
		}

		if (state == State::InDouble)
		{
			if (c == '\\' && i + 1 < input.size())
			{
				const char next = input[i + 1];
				if (next == '"' || next == '\\' || isSeparator(next))
				{
					current.push_back(next);
					++i;
					continue;
				}
			}
			if (c == '"')
			{
				state = State::Normal;
				continue;
			}
			current.push_back(c);
			continue;
		}

		// InSingle
		if (c == '\\' && i + 1 < input.size())
		{
			const char next = input[i + 1];
			if (next == '\'' || next == '\\')
			{
				current.push_back(next);
				++i;
				continue;
			}
		}
		if (c == '\'')
		{
			state = State::Normal;
			continue;
		}
		current.push_back(c);
	}

	if (state != State::Normal)
		LogWarning("Unterminated quote in command line; returning tokens parsed so far");

	if (inToken || state != State::Normal)
		tokens.push_back(std::move(current));

	return tokens;
}

std::filesystem::path tf2_bot_detector::Shell::BrowseForFolderDialog() {
    // FIXME: use qt or something instead of this; but I don't want to introduce an entire library
    // for one feature, so whatever. 
    // NOTE: this will block execution, and I kind of don't want to bother rewriting for async.
    FILE *pipe = popen("zenity --file-selection --directory", "r");

    if (!pipe) {
        LogError("failure to open pipe, zenity might not be installed.");
        return {};
    }

    char buffer[PATH_MAX];
    fgets(buffer, sizeof(buffer), pipe);
    pclose(pipe);
    std::string file_str = buffer;
    file_str.pop_back(); // remove trailing newline, #46 (shit fix but whatever)
    Log("Selected file: {}", file_str);

    return std::filesystem::path(file_str);
}

// selecting is not a feature in xdg-open
// cry about it
void tf2_bot_detector::Shell::ExploreToAndSelect(std::filesystem::path path) {
    xdg_open(path.c_str());
}

void tf2_bot_detector::Shell::ExploreTo(const std::filesystem::path& path) {
    xdg_open(path.c_str());
}

void tf2_bot_detector::Shell::OpenURL(const char* url) {
    xdg_open(url);
}

