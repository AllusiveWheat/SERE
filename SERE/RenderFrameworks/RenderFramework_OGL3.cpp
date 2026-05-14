#include "RenderFramework_OGL3.h"

#include <Imgui/imgui_impl_opengl3.h>
#include <Imgui/imgui_impl_sdl3.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

UINT WinResizeWidth = 0, WinResizeHeight = 0;


LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

RenderFramework_OGL3::RenderFramework_OGL3()
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
    }
    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
	if(!SDL_GL_LoadLibrary(nullptr)) { // Load the default OpenGL library
        SDL_Log("Failed to load OpenGL library: %s", SDL_GetError());
    }
	const char* glsl_version = "#version 130";
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

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
	if (!ImGui_ImplOpenGL3_Init(glsl_version)) {
		SDL_Log("Failed to initialize ImGui OpenGL3 backend");
	}

}

RenderFramework_OGL3::~RenderFramework_OGL3()
{
}

bool RenderFramework_OGL3::ShouldMainLoopRun()
{
	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		if (event.type == SDL_EVENT_KEY_DOWN)
		{
			if (event.key.key == SDLK_LALT || event.key.key == SDLK_RALT)
				break;
		}
		if (event.type == SDL_EVENT_QUIT)
			return false;
		if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window))
			return false;
		if (event.type == SDL_EVENT_WINDOW_RESIZED && event.window.windowID == SDL_GetWindowID(window))
		{
			// Release all outstanding references to the swap chain's buffers before resizing.
			WinResizeWidth = (UINT)event.window.data1; // Queue resize
			WinResizeHeight = (UINT)event.window.data2;
		}
		ImGui_ImplSDL3_ProcessEvent(&event);
	}
	return true;
}

bool RenderFramework_OGL3::ImGuiStartFrame()
{
	if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
	{
		SDL_Delay(10);
		return false;
	}

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	
	return true;
}

void RenderFramework_OGL3::ImGuiEndFrame()
{
	const ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
	const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
	glViewport(0, 0, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y);
	glClearColor(clear_color_with_alpha[0], clear_color_with_alpha[1], clear_color_with_alpha[2], clear_color_with_alpha[3]);
	glClear(GL_COLOR_BUFFER_BIT);
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	SDL_GL_SwapWindow(window);
}

void RenderFramework_OGL3::ImGuiDeInit()
{
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	SDL_GL_DestroyContext(glContext);
	SDL_DestroyWindow(window);
}

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
