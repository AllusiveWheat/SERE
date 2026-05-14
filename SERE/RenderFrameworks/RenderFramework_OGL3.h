#include "RenderFramework.h"
#include <SDL3/SDL.h>
#include <Windows.h>

class RenderFramework_OGL3 : public RenderFramework {
	struct Buffer {
		uint32_t id;
	};
	struct Texture {
		uint32_t id;
	};

public:
	RenderFramework_OGL3();
	~RenderFramework_OGL3();

	bool ShouldMainLoopRun();

	bool ImGuiStartFrame();
	void ImGuiEndFrame();

	void ImGuiDeInit();

	void RuiClearFrame();

	void DrawIndexed(uint32_t count, uint32_t start, size_t* resources);
	size_t CreateShaderDataBuffer(std::vector<ShaderSizeData_t> data);
	size_t CreateTextureFromData(void* data, uint32_t width, uint32_t height, uint16_t format, uint32_t pitch, uint32_t slicePitch);
	size_t LoadTexture(fs::path& path);

	void RuiWriteIndexBuffer(std::vector<uint16_t>& data);
	void RuiWriteVertexBuffer(std::vector<Vertex_t>& data);
	void RuiWriteStyleBuffer(std::vector<StyleDescriptorShader_t>& data);


	void RuiBindPipeline();
	void RuiLoad(int width, int height);
	void RuiReCreatePipeline(int width, int height);

	void* GetTextureView(size_t id);
	void* GetRuiView();
private:
	std::vector<Buffer> buffers;
	std::vector<Texture> textures;
	uint32_t ruiFBO;
	uint32_t ruiTexture;
	uint32_t ruiDepthStencil;
	SDL_Window* window;
	SDL_GLContext glContext;
};