#include "RenderFramework.h"
#include <gl/glew.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <Windows.h>
#include <gl/GL.h>


static const GLenum s_PakToGlFormat[] =
{
    GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,         // DXGI_FORMAT_BC1_UNORM
    GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT,  // DXGI_FORMAT_BC1_UNORM_SRGB
    GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,         // DXGI_FORMAT_BC2_UNORM
    GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT,  // DXGI_FORMAT_BC2_UNORM_SRGB
    GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,         // DXGI_FORMAT_BC3_UNORM
    GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT,  // DXGI_FORMAT_BC3_UNORM_SRGB
    GL_COMPRESSED_RED_RGTC1,                   // DXGI_FORMAT_BC4_UNORM
    GL_COMPRESSED_SIGNED_RED_RGTC1,            // DXGI_FORMAT_BC4_SNORM
    GL_COMPRESSED_RG_RGTC2,                    // DXGI_FORMAT_BC5_UNORM
    GL_COMPRESSED_SIGNED_RG_RGTC2,             // DXGI_FORMAT_BC5_SNORM
    GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT,     // DXGI_FORMAT_BC6H_UF16
    GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT,       // DXGI_FORMAT_BC6H_SF16
    GL_COMPRESSED_RGBA_BPTC_UNORM,             // DXGI_FORMAT_BC7_UNORM
    GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM,       // DXGI_FORMAT_BC7_UNORM_SRGB
    GL_RGBA32F,                                // DXGI_FORMAT_R32G32B32A32_FLOAT
    GL_RGBA32UI,                               // DXGI_FORMAT_R32G32B32A32_UINT
    GL_RGBA32I,                                // DXGI_FORMAT_R32G32B32A32_SINT
    GL_RGB32F,                                 // DXGI_FORMAT_R32G32B32_FLOAT
    GL_RGB32UI,                                // DXGI_FORMAT_R32G32B32_UINT
    GL_RGB32I,                                 // DXGI_FORMAT_R32G32B32_SINT
    GL_RGBA16F,                                // DXGI_FORMAT_R16G16B16A16_FLOAT
    GL_RGBA16,                                 // DXGI_FORMAT_R16G16B16A16_UNORM
    GL_RGBA16UI,                               // DXGI_FORMAT_R16G16B16A16_UINT
    GL_RGBA16_SNORM,                           // DXGI_FORMAT_R16G16B16A16_SNORM
    GL_RGBA16I,                                // DXGI_FORMAT_R16G16B16A16_SINT
    GL_RG32F,                                  // DXGI_FORMAT_R32G32_FLOAT
    GL_RG32UI,                                 // DXGI_FORMAT_R32G32_UINT
    GL_RG32I,                                  // DXGI_FORMAT_R32G32_SINT
    GL_RGB10_A2,                               // DXGI_FORMAT_R10G10B10A2_UNORM
    GL_RGB10_A2UI,                             // DXGI_FORMAT_R10G10B10A2_UINT
    GL_R11F_G11F_B10F,                         // DXGI_FORMAT_R11G11B10_FLOAT
    GL_RGBA8,                                  // DXGI_FORMAT_R8G8B8A8_UNORM
    GL_SRGB8_ALPHA8,                           // DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
    GL_RGBA8UI,                                // DXGI_FORMAT_R8G8B8A8_UINT
    GL_RGBA8_SNORM,                            // DXGI_FORMAT_R8G8B8A8_SNORM
    GL_RGBA8I,                                 // DXGI_FORMAT_R8G8B8A8_SINT
    GL_RG16F,                                  // DXGI_FORMAT_R16G16_FLOAT
    GL_RG16,                                   // DXGI_FORMAT_R16G16_UNORM
    GL_RG16UI,                                 // DXGI_FORMAT_R16G16_UINT
    GL_RG16_SNORM,                             // DXGI_FORMAT_R16G16_SNORM
    GL_RG16I,                                  // DXGI_FORMAT_R16G16_SINT
    GL_R32F,                                   // DXGI_FORMAT_R32_FLOAT
    GL_R32UI,                                  // DXGI_FORMAT_R32_UINT
    GL_R32I,                                   // DXGI_FORMAT_R32_SINT
    GL_RG8,                                    // DXGI_FORMAT_R8G8_UNORM
    GL_RG8UI,                                  // DXGI_FORMAT_R8G8_UINT
    GL_RG8_SNORM,                              // DXGI_FORMAT_R8G8_SNORM
    GL_RG8I,                                   // DXGI_FORMAT_R8G8_SINT
    GL_R16F,                                   // DXGI_FORMAT_R16_FLOAT
    GL_R16,                                    // DXGI_FORMAT_R16_UNORM
    GL_R16UI,                                  // DXGI_FORMAT_R16_UINT
    GL_R16_SNORM,                              // DXGI_FORMAT_R16_SNORM
    GL_R16I,                                   // DXGI_FORMAT_R16_SINT
    GL_R8,                                     // DXGI_FORMAT_R8_UNORM
    GL_R8UI,                                   // DXGI_FORMAT_R8_UINT
    GL_R8_SNORM,                               // DXGI_FORMAT_R8_SNORM
    GL_R8I,                                    // DXGI_FORMAT_R8_SINT
    GL_R8,                                     // DXGI_FORMAT_A8_UNORM          (*)
    GL_RGB9_E5,                                // DXGI_FORMAT_R9G9B9E5_SHAREDEXP
    GL_NONE,                                   // DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM (**)
    GL_DEPTH_COMPONENT32F,                     // DXGI_FORMAT_D32_FLOAT
    GL_DEPTH_COMPONENT16,                      // DXGI_FORMAT_D16_UNORM
};

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
    GLuint* ruiImageView;
	GLuint fbo;
	GLuint colorTexture;
	GLuint depthStencilTexture;
};