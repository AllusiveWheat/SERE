#include "RenderFramework_OGL3.h"
#include <Imgui/imgui_impl_opengl3.h>
#include <Imgui/imgui_impl_sdl3.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

UINT WinResizeWidth = 0, WinResizeHeight = 0;


RenderFramework_OGL3::RenderFramework_OGL3()
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
    }
    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_OPENGL;
	if(!SDL_GL_LoadLibrary(nullptr)) { // Load the default OpenGL library
        SDL_Log("Failed to load OpenGL library: %s", SDL_GetError());
    }
	const char* glsl_version = "#version 450 core";
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);

	window = SDL_CreateWindow("SERE", 1280, 800, window_flags);
	glContext = SDL_GL_CreateContext(window);
	if (glContext == nullptr) {
		SDL_Log("Failed to create OpenGL context: %s", SDL_GetError());
		SDL_Quit();
		return;
	}
    GLenum glewError = glewInit();
    if (glewError != GLEW_OK)
    {
        SDL_Log("Error initializing GLEW! %s\n", glewGetErrorString(glewError));
    }
	SDL_PropertiesID props = SDL_GetWindowProperties(window);
    SDL_GL_MakeCurrent(window, glContext);
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);
    SDL_StartTextInput(window);
    SDL_MaximizeWindow(window);
    SDL_SetHint(SDL_HINT_WINDOWS_ENABLE_MENU_MNEMONICS, "0"); // Disable ALT key mnemonics to avoid interfering with ImGui input handling
    
    ImGui_ImplSDL3_InitForOpenGL(window, glContext);
	if (!ImGui_ImplOpenGL3_Init(glsl_version)) {
		SDL_Log("Failed to initialize ImGui OpenGL3 backend");
	}

    fbo = 0;
    depthStencilTexture = 0;
    colorTexture = 0;

}

RenderFramework_OGL3::~RenderFramework_OGL3()
{
	SDL_GL_DestroyContext(glContext);
	SDL_DestroyWindow(window);
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
}

void RenderFramework_OGL3::RuiClearFrame()
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClearDepth(1.0);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, colorTexture);

}

void RenderFramework_OGL3::DrawIndexed(uint32_t count, uint32_t start, size_t * resources)
{
	//glDrawElementsInstanced(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, (void*)(start * sizeof(uint16_t)), 1);
}

size_t RenderFramework_OGL3::CreateShaderDataBuffer(std::vector<ShaderSizeData_t> data)
{
	
	return size_t();
}

size_t RenderFramework_OGL3::CreateTextureFromData(void* data, uint32_t width, uint32_t height, uint16_t format, uint32_t pitch, uint32_t slicePitch)
{
	size_t ret = textures.size();
	glGenTextures(1, &textures.emplace_back().id);
	glBindTexture(GL_TEXTURE_2D, textures.back().id);
	//glCompressedTexImage2D(GL_TEXTURE_2D, 0, s_PakToGlFormat[format], width, height, 0, pitch * height, data);
	if (format == 57) { // A8_UNORM, swizzle to get alpha in red channel and set other channels to 0
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_ZERO);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_ZERO);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_ZERO);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_RED);
	}
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	return ret;
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
{
    // --- Cleanup ---
    if (fbo) {
        glDeleteFramebuffers(1, &fbo);
        fbo = 0;
    }
    if (colorTexture) {
        glDeleteTextures(1, &colorTexture);
        colorTexture = 0;
    }
    if (depthStencilTexture) {
        glDeleteTextures(1, &depthStencilTexture);
        depthStencilTexture = 0;
    }

    // --- Color texture ---
    // Equivalent to: targetTexture + targetView + targetResourceView
    // (In GL, the texture itself IS the shader resource - no separate SRV needed)
    glGenTextures(1, &colorTexture);
    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA32F,
        width, height, 0,
        GL_RGBA, GL_FLOAT, nullptr
    );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    // --- Depth-stencil texture ---
    // Equivalent to: depthTexture + depthStencil (D24_UNORM_S8_UINT)
    glGenTextures(1, &depthStencilTexture);
    glBindTexture(GL_TEXTURE_2D, depthStencilTexture);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8,
        width, height, 0,
        GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr
    );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    // --- Framebuffer Object (FBO) ---
    // Equivalent to: CreateRenderTargetView + CreateDepthStencilView
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D, colorTexture, 0
    );
    glFramebufferTexture2D(
        GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
        GL_TEXTURE_2D, depthStencilTexture, 0
    );

    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // --- Rasterizer state ---
    // Equivalent to: D3D11_RASTERIZER_DESC
    glDisable(GL_CULL_FACE);                    // CullMode = NONE
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);  // FillMode = SOLID
    glDisable(GL_SCISSOR_TEST);                 // ScissorEnable = false
    glDisable(GL_MULTISAMPLE);                  // MultisampleEnable = false
    glDisable(GL_LINE_SMOOTH);                  // AntialiasedLineEnable = false
    glEnable(GL_DEPTH_CLAMP);                   // DepthClipEnable = false (GL 3.2+)
    glDisable(GL_POLYGON_OFFSET_FILL);          // DepthBias/SlopeScaledDepthBias = 0

    // --- Viewport ---
    glViewport(0, 0, width, height);
    //glDepthRange(0.0, 1.0);

    // --- Depth stencil state ---
    // Equivalent to: D3D11_DEPTH_STENCIL_DESC
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_ALWAYS);

    glEnable(GL_STENCIL_TEST);
    glStencilMaskSeparate(GL_FRONT_AND_BACK, 0xFF); // StencilWriteMask

    // Front face — StencilFunc: ALWAYS, Fail: KEEP, DepthFail: INCR, Pass: KEEP
    glStencilFuncSeparate(GL_FRONT, GL_ALWAYS, 0, 0xFF);
    glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_INCR, GL_KEEP);

    // Back face — StencilFunc: ALWAYS, Fail: KEEP, DepthFail: DECR, Pass: KEEP
    glStencilFuncSeparate(GL_BACK, GL_ALWAYS, 0, 0xFF);
    glStencilOpSeparate(GL_BACK, GL_KEEP, GL_DECR, GL_KEEP);

}

void* RenderFramework_OGL3::GetTextureView(size_t id)
{
	return nullptr;
}

void* RenderFramework_OGL3::GetRuiView()
{
	return (void*)colorTexture;
}
