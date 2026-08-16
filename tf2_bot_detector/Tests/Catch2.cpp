#include "Log.h"
#include "Tests.h"

#include <catch2/catch_session.hpp>

int tf2_bot_detector::RunTests()
{
	DebugLog(MH_SOURCE_LOCATION_CURRENT());
	return Catch::Session().run();
}
