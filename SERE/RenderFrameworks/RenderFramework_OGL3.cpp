#include "RenderFramework_OGL3.h"

#include <Imgui/imgui_impl_opengl3.h>
#include <Imgui/imgui_impl_sdl3.h>



bool CreateDeviceWGL() {
    return true;
}
void CleanupDeviceWGL() {

}
void ResetDeviceWGL() {

}
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

RenderFramework_OGL3::RenderFramework_OGL3()
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
    }
    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;

    window = SDL_CreateWindow("SERE", 1280, 800, window_flags);
    glContext = SDL_GL_CreateContext(window);
    SDL_PropertiesID props = SDL_GetWindowProperties(window);
   
   
    SDL_GL_MakeCurrent(window, glContext);
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);
    SDL_MaximizeWindow(window);
    SDL_SetHint(SDL_HINT_WINDOWS_ENABLE_MENU_MNEMONICS, "0"); // Disable ALT key mnemonics to avoid interfering with ImGui input handling
    
    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForOpenGL(window, glContext);
    ImGui_ImplOpenGL3_Init("#version 450 core");

}

RenderFramework_OGL3::~RenderFramework_OGL3()
{
}

bool RenderFramework_OGL3::ShouldMainLoopRun()
{
	return false;
}

bool RenderFramework_OGL3::ImGuiStartFrame()
{
	return false;
}

void RenderFramework_OGL3::ImGuiEndFrame()
{}

void RenderFramework_OGL3::ImGuiDeInit()
{}

void RenderFramework_OGL3::RuiClearFrame()
{}

void RenderFramework_OGL3::DrawIndexed(uint32_t count, uint32_t start, size_t * resources)
{}

size_t RenderFramework_OGL3::CreateShaderDataBuffer(std::vector<ShaderSizeData_t> data)
{
	return size_t();
}

size_t RenderFramework_OGL3::CreateTextureFromData(void* data, uint32_t width, uint32_t height, uint16_t format, uint32_t pitch, uint32_t slicePitch)
{
	return size_t();
}

size_t RenderFramework_OGL3::LoadTexture(fs::path& path)
{
	return size_t();
}

void RenderFramework_OGL3::RuiWriteIndexBuffer(std::vector<uint16_t>& data)
{}

void RenderFramework_OGL3::RuiWriteVertexBuffer(std::vector<Vertex_t>&data)
{}

void RenderFramework_OGL3::RuiWriteStyleBuffer(std::vector<StyleDescriptorShader_t>&data)
{}

void RenderFramework_OGL3::RuiBindPipeline()
{}

void RenderFramework_OGL3::RuiLoad(int width, int height)
{}

void RenderFramework_OGL3::RuiReCreatePipeline(int width, int height)
{}

void* RenderFramework_OGL3::GetTextureView(size_t id)
{
	return nullptr;
}

void* RenderFramework_OGL3::GetRuiView()
{
	return nullptr;
}
