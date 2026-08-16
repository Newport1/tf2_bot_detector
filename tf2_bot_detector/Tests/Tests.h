#pragma once

#ifdef TF2BD_ENABLE_TESTS
namespace tf2_bot_detector
{
	// Anything on the command line after --run-tests is forwarded to Catch2, so
	// test selection works the way Catch2 documents it:
	//   tf2_bot_detector_cli --run-tests "[realsteam]"
	//   tf2_bot_detector_cli --run-tests --list-tests
	// Called with no arguments it just runs the default (non-hidden) set.
	int RunTests(int argc = 0, const char* const* argv = nullptr);
}
#endif
