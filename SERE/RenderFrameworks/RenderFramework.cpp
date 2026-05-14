#include "RenderFrameworks/RenderFramework.h"
#include "RenderFrameworks/RenderFramework_Dx11.h"
#include "RenderFrameworks/RenderFramework_OGL3.h"
std::unique_ptr<RenderFramework> g_renderFramework;

#define USE_OGL

void CreateRenderFramework(char** argv, int argc) {
#ifndef USE_OGL
	g_renderFramework = std::make_unique<RenderFramework_Dx11>();
#else
	g_renderFramework = std::make_unique< RenderFramework_OGL3>();
#endif // !USE_OGL
}