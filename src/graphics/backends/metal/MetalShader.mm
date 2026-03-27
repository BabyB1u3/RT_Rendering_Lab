#include "MetalShader.h"

#import <Metal/Metal.h>

#include <array>
#include <unordered_map>
#include <vector>
#include <functional>

#include <glm/gtc/type_ptr.hpp>
#include <json.hpp>

#include "core/FileSystem.h"
#include "core/diagnostics/LogCategories.h"
#include "core/diagnostics/LogMacros.h"
#include "graphics/GraphicsDevice.h"
#include "graphics/RenderTypes.h"
#include "graphics/backends/metal/MetalGraphicsDevice.h"
#include "graphics/backends/metal/MetalRenderCommand.h"

// --- PSO cache key ---

struct PSOKey
{
	uint64_t vertLayoutHash  = 0;
	std::array<uint32_t, 4> colorFmts = {0, 0, 0, 0};
	uint32_t colorAttachmentCount = 0;
	uint32_t depthFmt        = 0;
	bool     blendEnabled    = false;
	bool     depthTest       = true;
	bool     depthWrite      = true;
	bool     cullEnabled     = true;
	bool     cullFront       = false;

	bool operator==(const PSOKey &o) const
	{
		return vertLayoutHash == o.vertLayoutHash &&
		       colorFmts      == o.colorFmts      &&
		       colorAttachmentCount == o.colorAttachmentCount &&
		       depthFmt       == o.depthFmt       &&
		       blendEnabled   == o.blendEnabled   &&
		       depthTest      == o.depthTest      &&
		       depthWrite     == o.depthWrite     &&
		       cullEnabled    == o.cullEnabled    &&
		       cullFront      == o.cullFront;
	}
};

struct PSOKeyHash
{
	size_t operator()(const PSOKey &k) const
	{
		size_t h = std::hash<uint64_t>{}(k.vertLayoutHash);
		for (uint32_t colorFmt : k.colorFmts)
			h ^= std::hash<uint32_t>{}(colorFmt)    + 0x9e3779b9 + (h << 6) + (h >> 2);
		h ^= std::hash<uint32_t>{}(k.colorAttachmentCount) + 0x9e3779b9 + (h << 6) + (h >> 2);
		h ^= std::hash<uint32_t>{}(k.depthFmt)     + 0x9e3779b9 + (h << 6) + (h >> 2);
		h ^= std::hash<bool>{}(k.blendEnabled)      + 0x9e3779b9 + (h << 6) + (h >> 2);
		h ^= std::hash<bool>{}(k.depthTest)         + 0x9e3779b9 + (h << 6) + (h >> 2);
		h ^= std::hash<bool>{}(k.depthWrite)        + 0x9e3779b9 + (h << 6) + (h >> 2);
		h ^= std::hash<bool>{}(k.cullEnabled)       + 0x9e3779b9 + (h << 6) + (h >> 2);
		h ^= std::hash<bool>{}(k.cullFront)         + 0x9e3779b9 + (h << 6) + (h >> 2);
		return h;
	}
};

// --- Reflection types ---

struct UniformInfo
{
	uint32_t offset = 0;
	uint32_t size   = 0;
};

using ReflectionMap = std::unordered_map<std::string, UniformInfo>;

// --- Impl ---

static constexpr size_t kStagingBufferSize = 4096;

struct MetalShader::Impl
{
	id<MTLLibrary>  library;
	id<MTLFunction> vertexFunction;
	id<MTLFunction> fragmentFunction;

	std::string name;

	std::unordered_map<PSOKey, id<MTLRenderPipelineState>, PSOKeyHash> psoCache;

	// CPU staging buffers for named uniforms (Set* calls)
	std::vector<uint8_t> vertexStaging;    // zeroed; offsets from vertexReflection
	std::vector<uint8_t> fragmentStaging;  // zeroed; offsets from fragmentReflection
	bool                 vertexDirty   = false;
	bool                 fragmentDirty = false;

	// Name - byte offset/size within the staging buffer
	ReflectionMap vertexReflection;
	ReflectionMap fragmentReflection;

	// Explicit uniform blocks (SetUniformBlock)
	std::unordered_map<uint32_t, std::vector<uint8_t>> uniformBlocks;

	// Metal buffer binding indices
	uint32_t vertexUniformBinding   = 1;   // default; overridden by JSON sidecar
	uint32_t fragmentUniformBinding = 0;   // default; overridden by JSON sidecar

	// Slang assigns MSL texture indices 0-based, while GLSL bindings include UBOs.
	// textureBindingBase = number of UBO bindings before the first texture binding.
	// Metal texture index = C++ slot - textureBindingBase.
	// Default: 1 (one GlobalParams CB). Overridable via sidecar "textureBindingBase".
	uint32_t textureBindingBase = 1;
};

// --- Helpers ---

static uint64_t HashVertexDescriptor(MTLVertexDescriptor *desc)
{
	uint64_t h = 0;
	for (int i = 0; i < 31; ++i)
	{
		MTLVertexAttributeDescriptor *attr = desc.attributes[i];
		if (attr.format == MTLVertexFormatInvalid) continue;
		h ^= (uint64_t)attr.format      << (i % 8 * 5);
		h ^= (uint64_t)attr.offset      << 8;
		h ^= (uint64_t)attr.bufferIndex << 16;
	}
	for (int i = 0; i < 31; ++i)
	{
		MTLVertexBufferLayoutDescriptor *layout = desc.layouts[i];
		if (layout.stride == 0) continue;
		h ^= (uint64_t)layout.stride << (i % 8 * 3 + 32);
	}
	return h;
}

static void LoadReflectionSidecar(const std::string &name,
                                  ReflectionMap &vertex, ReflectionMap &fragment,
                                  uint32_t &vertexBinding, uint32_t &fragmentBinding,
                                  uint32_t &textureBase)
{
	auto path = FileSystem::GetCompiledShaderDir() / "metal" / (name + ".reflect.json");
	if (!FileSystem::Exists(path))
		return;

	const auto reflectionText = FileSystem::ReadTextFile(path);
	if (!reflectionText)
	{
		LOG_WARN_CAT(LogCategory::Shader, "MetalShader '{}': failed to read reflection sidecar '{}'",
		             name, path.string());
		return;
	}

	try
	{
		auto json = nlohmann::json::parse(*reflectionText);
		auto &entry = json.at(name);

		if (entry.contains("vertexUniformBinding"))
			vertexBinding = entry["vertexUniformBinding"].get<uint32_t>();
		if (entry.contains("fragmentUniformBinding"))
			fragmentBinding = entry["fragmentUniformBinding"].get<uint32_t>();
		if (entry.contains("textureBindingBase"))
			textureBase = entry["textureBindingBase"].get<uint32_t>();

		if (entry.contains("vertex"))
		{
			for (auto &[uname, uinfo] : entry["vertex"].items())
				vertex[uname] = { uinfo["offset"].get<uint32_t>(), uinfo["size"].get<uint32_t>() };
		}
		if (entry.contains("fragment"))
		{
			for (auto &[uname, uinfo] : entry["fragment"].items())
				fragment[uname] = { uinfo["offset"].get<uint32_t>(), uinfo["size"].get<uint32_t>() };
		}
		LOG_TRACE_CAT(LogCategory::Shader, "MetalShader '{}': loaded reflection sidecar", name);
	}
	catch (const std::exception &e)
	{
		LOG_WARN_CAT(LogCategory::Shader, "MetalShader '{}': failed to parse reflection sidecar: {}", name, e.what());
	}
}

// --- Factories ---

Ref<MetalShader> MetalShader::CreateFromMSLSource(const std::string &name,
                                                   const std::string &source)
{
	auto *dev    = static_cast<MetalGraphicsDevice *>(GetDevice().get());
	id<MTLDevice> device = (__bridge id<MTLDevice>)dev->GetMTLDevice();

	NSError   *error  = nil;
	NSString  *src    = [NSString stringWithUTF8String:source.c_str()];
	id<MTLLibrary> lib = [device newLibraryWithSource:src options:nil error:&error];
	if (!lib)
	{
		LOG_ERROR_CAT(LogCategory::Shader, "MetalShader '{}': MSL compilation failed: {}",
		              name, [[error localizedDescription] UTF8String]);
		return nullptr;
	}

	id<MTLFunction> vertFn = [lib newFunctionWithName:@"vertexMain"];
	id<MTLFunction> fragFn = [lib newFunctionWithName:@"fragmentMain"];

	if (!vertFn) LOG_WARN_CAT(LogCategory::Shader, "MetalShader '{}': 'vertexMain' not found in library", name);
	if (!fragFn) LOG_WARN_CAT(LogCategory::Shader, "MetalShader '{}': 'fragmentMain' not found in library", name);

	auto *shader = new MetalShader();
	shader->m_Impl = std::make_unique<Impl>();
	shader->m_Impl->library          = lib;
	shader->m_Impl->vertexFunction   = vertFn;
	shader->m_Impl->fragmentFunction = fragFn;
	shader->m_Impl->name             = name;
	shader->m_Impl->vertexStaging.assign(kStagingBufferSize, 0);
	shader->m_Impl->fragmentStaging.assign(kStagingBufferSize, 0);

	LoadReflectionSidecar(name,
	                      shader->m_Impl->vertexReflection,
	                      shader->m_Impl->fragmentReflection,
	                      shader->m_Impl->vertexUniformBinding,
	                      shader->m_Impl->fragmentUniformBinding,
	                      shader->m_Impl->textureBindingBase);

	LOG_INFO_CAT(LogCategory::Shader, "MetalShader: loaded '{}'", name);
	return Ref<MetalShader>(shader);
}

Ref<MetalShader> MetalShader::CreateFromCompiledMSL(const std::string &name)
{
	auto path = FileSystem::GetCompiledShaderDir() / "metal" / (name + ".metal");
	if (!FileSystem::Exists(path))
	{
		LOG_ERROR_CAT(LogCategory::Shader,
		              "Compiled MSL shader missing: {}. Enable GLAB_SHADER_TARGET_METAL and rebuild the CompileShaders target.",
		              path.string());
		return nullptr;
	}

	const auto source = FileSystem::ReadTextFile(path);
	if (!source)
	{
		LOG_ERROR_CAT(LogCategory::Shader, "Compiled MSL load failed ({}): could not read '{}'",
		              name, path.string());
		return nullptr;
	}

	return CreateFromMSLSource(name, *source);
}

// --- Destructor ---

MetalShader::~MetalShader() = default;

// --- IShader ---

void MetalShader::Bind() const
{
	// Register this shader as the "current" on the Metal render command so that
	// DrawIndexed can retrieve the vertex/fragment functions for PSO creation.
	auto *dev = static_cast<MetalGraphicsDevice *>(GetDevice().get());
	dev->GetMetalRenderCommand()->SetCurrentShader(const_cast<MetalShader *>(this));
}

const std::string &MetalShader::GetName() const
{
	return m_Impl->name;
}

// --- Named uniform setters ---

void MetalShader::WriteToStagingBuffer(const std::string &name, const void *data, uint32_t size)
{
	// Check vertex stage
	auto vit = m_Impl->vertexReflection.find(name);
	if (vit != m_Impl->vertexReflection.end())
	{
		const auto &info = vit->second;
		if (info.offset + size <= kStagingBufferSize)
		{
			memcpy(m_Impl->vertexStaging.data() + info.offset, data, size);
			m_Impl->vertexDirty = true;
		}
	}

	// Check fragment stage
	auto fit = m_Impl->fragmentReflection.find(name);
	if (fit != m_Impl->fragmentReflection.end())
	{
		const auto &info = fit->second;
		if (info.offset + size <= kStagingBufferSize)
		{
			memcpy(m_Impl->fragmentStaging.data() + info.offset, data, size);
			m_Impl->fragmentDirty = true;
		}
	}
}

void MetalShader::SetInt(const std::string &name, int value)
{
	WriteToStagingBuffer(name, &value, sizeof(int));
}

void MetalShader::SetIntArray(const std::string &name, const int *values, uint32_t count)
{
	WriteToStagingBuffer(name, values, count * sizeof(int));
}

void MetalShader::SetBool(const std::string &name, bool value)
{
	int v = value ? 1 : 0;
	WriteToStagingBuffer(name, &v, sizeof(int));
}

void MetalShader::SetFloat(const std::string &name, float value)
{
	WriteToStagingBuffer(name, &value, sizeof(float));
}

void MetalShader::SetFloat2(const std::string &name, const glm::vec2 &value)
{
	WriteToStagingBuffer(name, glm::value_ptr(value), sizeof(glm::vec2));
}

void MetalShader::SetFloat3(const std::string &name, const glm::vec3 &value)
{
	WriteToStagingBuffer(name, glm::value_ptr(value), sizeof(glm::vec3));
}

void MetalShader::SetFloat4(const std::string &name, const glm::vec4 &value)
{
	WriteToStagingBuffer(name, glm::value_ptr(value), sizeof(glm::vec4));
}

void MetalShader::SetMat3(const std::string &name, const glm::mat3 &value)
{
	WriteToStagingBuffer(name, glm::value_ptr(value), sizeof(glm::mat3));
}

void MetalShader::SetMat4(const std::string &name, const glm::mat4 &value)
{
	WriteToStagingBuffer(name, glm::value_ptr(value), sizeof(glm::mat4));
}

void MetalShader::SetUniformBlock(uint32_t binding, const void *data, uint32_t size)
{
	auto &block = m_Impl->uniformBlocks[binding];
	block.resize(size);
	memcpy(block.data(), data, size);
}

// --- Metal-internal ---

void *MetalShader::GetOrCreatePSO(void *mtlDevice,
                                  void *mtlVertDescriptor,
                                  const std::array<uint32_t, 4> &colorPixelFormats,
                                  uint32_t colorAttachmentCount,
                                  uint32_t depthPixelFormat,
                                  const PipelineState &ps)
{
	PSOKey key;
	key.colorFmts    = colorPixelFormats;
	key.colorAttachmentCount = colorAttachmentCount;
	key.depthFmt     = depthPixelFormat;
	key.blendEnabled = ps.BlendEnabled;
	key.depthTest    = ps.DepthTestEnabled;
	key.depthWrite   = ps.DepthWriteEnabled;
	key.cullEnabled  = ps.CullFaceEnabled;
	key.cullFront    = ps.CullFront;

	MTLVertexDescriptor *vertDesc = (__bridge MTLVertexDescriptor *)mtlVertDescriptor;
	key.vertLayoutHash = vertDesc ? HashVertexDescriptor(vertDesc) : 0;

	auto it = m_Impl->psoCache.find(key);
	if (it != m_Impl->psoCache.end())
		return (__bridge void *)it->second;

	// Build pipeline descriptor
	id<MTLDevice> device = (__bridge id<MTLDevice>)mtlDevice;
	MTLRenderPipelineDescriptor *desc = [MTLRenderPipelineDescriptor new];
	desc.label            = [NSString stringWithUTF8String:m_Impl->name.c_str()];
	desc.vertexFunction   = m_Impl->vertexFunction;
	desc.fragmentFunction = m_Impl->fragmentFunction;
	desc.vertexDescriptor = vertDesc;

	// Color attachments
	for (uint32_t index = 0; index < colorAttachmentCount && index < colorPixelFormats.size(); ++index)
	{
		uint32_t colorPixelFormat = colorPixelFormats[index];
		if (colorPixelFormat == (uint32_t)MTLPixelFormatInvalid)
			continue;

		desc.colorAttachments[index].pixelFormat = (MTLPixelFormat)colorPixelFormat;
		if (ps.BlendEnabled)
		{
			desc.colorAttachments[index].blendingEnabled             = YES;
			desc.colorAttachments[index].sourceRGBBlendFactor        = MTLBlendFactorSourceAlpha;
			desc.colorAttachments[index].destinationRGBBlendFactor   = MTLBlendFactorOneMinusSourceAlpha;
			desc.colorAttachments[index].sourceAlphaBlendFactor      = MTLBlendFactorOne;
			desc.colorAttachments[index].destinationAlphaBlendFactor = MTLBlendFactorZero;
		}
	}

	// Depth/stencil attachment pixel format
	MTLPixelFormat depthFmt = (MTLPixelFormat)depthPixelFormat;
	if (depthFmt == MTLPixelFormatDepth32Float || depthFmt == MTLPixelFormatDepth32Float_Stencil8)
	{
		desc.depthAttachmentPixelFormat = depthFmt;
		if (depthFmt == MTLPixelFormatDepth32Float_Stencil8)
			desc.stencilAttachmentPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
	}

	NSError *error = nil;
	id<MTLRenderPipelineState> pso = [device newRenderPipelineStateWithDescriptor:desc error:&error];
	if (!pso)
	{
		LOG_ERROR_CAT(LogCategory::Shader, "MetalShader '{}': PSO creation failed: {}",
		              m_Impl->name, [[error localizedDescription] UTF8String]);
		return nullptr;
	}

	m_Impl->psoCache[key] = pso;
	LOG_TRACE_CAT(LogCategory::Shader, "MetalShader '{}': created new PSO (cache size: {})",
	              m_Impl->name, m_Impl->psoCache.size());
	return (__bridge void *)pso;
}

uint32_t MetalShader::GetTextureBindingBase() const
{
	return m_Impl->textureBindingBase;
}

void MetalShader::FlushUniforms(void *mtlEncoder)
{
	id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)mtlEncoder;

	// Named uniforms - uploaded only when reflection data is present and buffer is dirty
	if (!m_Impl->vertexReflection.empty() && m_Impl->vertexDirty)
	{
		[encoder setVertexBytes:m_Impl->vertexStaging.data()
		                length:kStagingBufferSize
		               atIndex:m_Impl->vertexUniformBinding];
		m_Impl->vertexDirty = false;
	}
	if (!m_Impl->fragmentReflection.empty() && m_Impl->fragmentDirty)
	{
		[encoder setFragmentBytes:m_Impl->fragmentStaging.data()
		                  length:kStagingBufferSize
		                 atIndex:m_Impl->fragmentUniformBinding];
		m_Impl->fragmentDirty = false;
	}

	// Explicit uniform blocks (SetUniformBlock) - always uploaded
	for (const auto &[binding, data] : m_Impl->uniformBlocks)
	{
		uint32_t slotIndex = kUniformBaseSlot + binding;
		[encoder setVertexBytes:data.data()   length:data.size() atIndex:slotIndex];
		[encoder setFragmentBytes:data.data() length:data.size() atIndex:slotIndex];
	}
}
