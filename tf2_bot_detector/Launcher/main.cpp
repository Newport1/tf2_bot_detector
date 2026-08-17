#include "../DLLMain.h"

// Only the GUI launcher is a /subsystem:windows target; tf2_bot_detector_cli is a
// console exe and needs a real main(). Keying off WIN32 gave the CLI wWinMain only,
// so it failed to link on Windows. TF2BD_LAUNCHER_USE_WINMAIN is set on the launcher
// target alone, which is exactly the distinction we want.
#ifdef TF2BD_LAUNCHER_USE_WINMAIN
#include <Windows.h>
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nShowCmd)
{
	return tf2_bot_detector::RunProgram();
}
#else
int main(int argc, const char** argv)
{
	return tf2_bot_detector::RunProgram(argc, argv);
}
#endif
