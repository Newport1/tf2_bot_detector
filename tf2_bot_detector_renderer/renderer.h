#if BD_RENDERER_MODE == OPENGL
#include "sdl2opengl.h"
using TF2BDRenderer = TF2BotDetectorSDLRenderer;
#else
#error "Invalid Renderer mode!"
#endif
