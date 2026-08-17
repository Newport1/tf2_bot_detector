#include "Log.h"
#include "Tests.h"

#include <catch2/catch_session.hpp>

#include <vector>

int tf2_bot_detector::RunTests(int argc, const char* const* argv)
{
	DebugLog(MH_SOURCE_LOCATION_CURRENT());

	Catch::Session session;

	// Catch2 expects a conventional argv with the program name in slot 0. The
	// caller hands us only the arguments that followed --run-tests, so synthesize
	// slot 0 and forward the rest. Without this, test selection (tags, --list-tests)
	// is unreachable and hidden tags like [.realsteam] can never be run.
	if (argc > 0 && argv)
	{
		std::vector<const char*> args;
		args.reserve(static_cast<size_t>(argc) + 1);
		args.push_back("tf2_bot_detector_cli --run-tests");
		for (int i = 0; i < argc; ++i)
			args.push_back(argv[i]);

		if (const int result = session.applyCommandLine(static_cast<int>(args.size()), args.data()))
			return result;
	}

	return session.run();
}
