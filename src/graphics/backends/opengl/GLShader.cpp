#include "GLShader.h"

#include <algorithm>
#include <array>
#include <optional>
#include <regex>
#include <string>
#include <vector>

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <json.hpp>

#include "core/FileSystem.h"
#include "core/diagnostics/LogCategories.h"
#include "core/diagnostics/LogMacros.h"
#include "graphics/backends/opengl/GLCast.h"
#include "graphics/backends/opengl/GLUniformBuffer.h"
#include "graphics/interfaces/ITexture2D.h"

namespace
{

ShaderUniformValueType MapGLUniformType(GLenum type)
{
	switch (type)
	{
	case GL_BOOL:       return ShaderUniformValueType::Bool;
	case GL_INT:        return ShaderUniformValueType::Int;
	case GL_INT_VEC2:   return ShaderUniformValueType::Int2;
	case GL_INT_VEC3:   return ShaderUniformValueType::Int3;
	case GL_INT_VEC4:   return ShaderUniformValueType::Int4;
	case GL_FLOAT:      return ShaderUniformValueType::Float;
	case GL_FLOAT_VEC2: return ShaderUniformValueType::Float2;
	case GL_FLOAT_VEC3: return ShaderUniformValueType::Float3;
	case GL_FLOAT_VEC4: return ShaderUniformValueType::Float4;
	case GL_FLOAT_MAT3: return ShaderUniformValueType::Mat3;
	case GL_FLOAT_MAT4: return ShaderUniformValueType::Mat4;
	default:            return ShaderUniformValueType::Unknown;
	}
}


std::string TrimTrailingNull(std::string value)
{
	if (!value.empty() && value.back() == '\0')
		value.pop_back();
	return value;
}

std::optional<ShaderBindingPoint> ParseLogicalBindingPoint(const nlohmann::json &bindingJson)
{
	if (!bindingJson.is_object() || !bindingJson.contains("index"))
		return std::nullopt;

	return ShaderBindingPoint{
		bindingJson.value("space", 0u),
		bindingJson.value("index", 0u)
	};
}

ShaderUniformValueType ParseReflectionValueType(const nlohmann::json &typeJson)
{
	if (!typeJson.is_object())
		return ShaderUniformValueType::Unknown;

	const std::string kind = typeJson.value("kind", std::string{});
	if (kind == "scalar")
	{
		const std::string scalarType = typeJson.value("scalarType", std::string{});
		if (scalarType == "bool")
			return ShaderUniformValueType::Bool;
		if (scalarType == "float32")
			return ShaderUniformValueType::Float;
		if (scalarType == "int32" || scalarType == "uint32")
			return ShaderUniformValueType::Int;
		return ShaderUniformValueType::Unknown;
	}

	if (kind == "vector")
	{
		const uint32_t elementCount = typeJson.value("elementCount", 0u);
		const ShaderUniformValueType elementType =
			ParseReflectionValueType(typeJson.value("elementType", nlohmann::json::object()));

		if (elementType == ShaderUniformValueType::Float)
		{
			switch (elementCount)
			{
			case 2: return ShaderUniformValueType::Float2;
			case 3: return ShaderUniformValueType::Float3;
			case 4: return ShaderUniformValueType::Float4;
			default: return ShaderUniformValueType::Unknown;
			}
		}

		if (elementType == ShaderUniformValueType::Int)
		{
			switch (elementCount)
			{
			case 2: return ShaderUniformValueType::Int2;
			case 3: return ShaderUniformValueType::Int3;
			case 4: return ShaderUniformValueType::Int4;
			default: return ShaderUniformValueType::Unknown;
			}
		}

		return ShaderUniformValueType::Unknown;
	}

	if (kind == "matrix")
	{
		const uint32_t rowCount = typeJson.value("rowCount", 0u);
		const uint32_t columnCount = typeJson.value("columnCount", 0u);
		const ShaderUniformValueType elementType =
			ParseReflectionValueType(typeJson.value("elementType", nlohmann::json::object()));
		if (elementType != ShaderUniformValueType::Float)
			return ShaderUniformValueType::Unknown;

		if (rowCount == 3 && columnCount == 3)
			return ShaderUniformValueType::Mat3;
		if (rowCount == 4 && columnCount == 4)
			return ShaderUniformValueType::Mat4;
	}

	return ShaderUniformValueType::Unknown;
}

uint32_t ComputeReflectedLayoutSize(const ShaderUniformBlockLayout &layout)
{
	uint32_t size = 0;
	for (const auto &field : layout.GetFields())
		size = std::max(size, field.Offset + field.Size);

	return size;
}

void AppendReflectedStructFields(const nlohmann::json &typeJson,
                                 ShaderUniformBlockLayout &layout,
                                 uint32_t baseOffset = 0)
{
	if (!typeJson.is_object())
		return;

	const std::string kind = typeJson.value("kind", std::string{});
	if (kind == "constantBuffer" ||
	    kind == "parameterBlock" ||
	    kind == "textureBuffer" ||
	    kind == "shaderStorageBuffer")
	{
		AppendReflectedStructFields(typeJson.value("elementType", nlohmann::json::object()),
		                            layout,
		                            baseOffset);
		return;
	}

	if (kind != "struct" || !typeJson.contains("fields") || !typeJson["fields"].is_array())
		return;

	for (const auto &fieldJson : typeJson["fields"])
	{
		const auto fieldType = fieldJson.value("type", nlohmann::json::object());
		const auto fieldBinding = fieldJson.value("binding", nlohmann::json::object());
		const uint32_t fieldOffset = baseOffset + fieldBinding.value("offset", 0u);
		const uint32_t fieldSize = fieldBinding.value("size", 0u);

		const std::string fieldKind = fieldType.value("kind", std::string{});
		if (fieldKind == "struct" ||
		    fieldKind == "constantBuffer" ||
		    fieldKind == "parameterBlock")
		{
			AppendReflectedStructFields(fieldType, layout, fieldOffset);
			continue;
		}

		const std::string fieldName = fieldJson.value("name", std::string{});
		if (fieldName.empty() || fieldSize == 0)
			continue;

		layout.AddField({
			fieldName,
			fieldOffset,
			fieldSize,
			ParseReflectionValueType(fieldType)
		});
	}
}

uint32_t ResolveGLBufferBindingIndex(const ShaderBackendBindingMap &backendBindings,
                                     ShaderBindingPoint logicalBinding)
{
	auto it = backendBindings.find(logicalBinding);
	if (it != backendBindings.end() && it->second.BufferIndex.has_value())
		return *it->second.BufferIndex;

	return logicalBinding.Binding;
}

uint32_t ResolveGLTextureBindingIndex(const ShaderBackendBindingMap &backendBindings,
                                      ShaderBindingPoint logicalBinding)
{
	auto it = backendBindings.find(logicalBinding);
	if (it != backendBindings.end() && it->second.TextureIndex.has_value())
		return *it->second.TextureIndex;

	return logicalBinding.Binding;
}

} // namespace

GLShader::GLShader(uint32_t program, std::string name)
	: m_RendererID(program), m_Name(std::move(name))
{
	ReflectUniformBlocks();
}

GLShader::~GLShader()
{
	for (auto &[binding, ubo] : m_UBOCache)
	{
		if (ubo != 0)
			glDeleteBuffers(1, &ubo);
	}
	if (m_RendererID != 0)
		glDeleteProgram(m_RendererID);
}

GLShader::GLShader(GLShader &&other) noexcept
	: m_RendererID(other.m_RendererID),
	  m_Name(std::move(other.m_Name)),
	  m_UniformLocationCache(std::move(other.m_UniformLocationCache)),
	  m_UBOCache(std::move(other.m_UBOCache)),
	  m_BlockLayouts(std::move(other.m_BlockLayouts)),
	  m_ResourceLayouts(std::move(other.m_ResourceLayouts)),
	  m_BackendBindings(std::move(other.m_BackendBindings))
{
	other.m_RendererID = 0;
}

GLShader &GLShader::operator=(GLShader &&other) noexcept
{
	if (this == &other)
		return *this;

	for (auto &[binding, ubo] : m_UBOCache)
	{
		if (ubo != 0)
			glDeleteBuffers(1, &ubo);
	}
	if (m_RendererID != 0)
		glDeleteProgram(m_RendererID);

	m_RendererID = other.m_RendererID;
	m_Name = std::move(other.m_Name);
	m_UniformLocationCache = std::move(other.m_UniformLocationCache);
	m_UBOCache = std::move(other.m_UBOCache);
	m_BlockLayouts = std::move(other.m_BlockLayouts);
	m_ResourceLayouts = std::move(other.m_ResourceLayouts);
	m_BackendBindings = std::move(other.m_BackendBindings);

	other.m_RendererID = 0;
	return *this;
}

uint32_t GLShader::CompileStage(uint32_t stage, const std::string &source, const std::string &debugName)
{
	GLuint shader = glCreateShader(stage);
	const GLchar *src = source.c_str();
	glShaderSource(shader, 1, &src, nullptr);
	glCompileShader(shader);

	GLint compiled = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (compiled == GL_FALSE)
	{
		GLint maxLength = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

		std::vector<GLchar> infoLog(maxLength);
		glGetShaderInfoLog(shader, maxLength, &maxLength, infoLog.data());
		glDeleteShader(shader);

		LOG_ERROR_CAT(LogCategory::Shader, "Shader compilation failed ({}): {}", debugName, infoLog.data());
		return 0;
	}

	LOG_TRACE_CAT(LogCategory::Shader, "Shader stage compiled: {}", debugName);
	return shader;
}

uint32_t GLShader::LinkProgram(const std::string &name, const std::vector<uint32_t> &shaderIDs)
{
	GLuint program = glCreateProgram();

	for (uint32_t id : shaderIDs)
		glAttachShader(program, id);

	glLinkProgram(program);

	GLint linked = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &linked);
	if (linked == GL_FALSE)
	{
		GLint maxLength = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

		std::vector<GLchar> infoLog(maxLength);
		glGetProgramInfoLog(program, maxLength, &maxLength, infoLog.data());

		for (uint32_t id : shaderIDs)
			glDeleteShader(id);

		glDeleteProgram(program);

		LOG_ERROR_CAT(LogCategory::Shader, "Shader link failed ({}): {}", name, infoLog.data());
		return 0;
	}

	for (uint32_t id : shaderIDs)
	{
		glDetachShader(program, id);
		glDeleteShader(id);
	}

	LOG_INFO_CAT(LogCategory::Shader, "Shader linked: {}", name);
	return program;
}

Ref<GLShader> GLShader::CreateFromSource(
	const std::string &name,
	const std::string &vertexSource,
	const std::string &fragmentSource,
	const std::string &geometrySource)
{
	std::vector<uint32_t> shaderIDs;

	const uint32_t vertexShader = CompileStage(GL_VERTEX_SHADER, vertexSource, name + " [vertex]");
	if (vertexShader == 0)
		return nullptr;
	shaderIDs.push_back(vertexShader);

	const uint32_t fragmentShader = CompileStage(GL_FRAGMENT_SHADER, fragmentSource, name + " [fragment]");
	if (fragmentShader == 0)
	{
		for (uint32_t id : shaderIDs)
			glDeleteShader(id);
		return nullptr;
	}
	shaderIDs.push_back(fragmentShader);

	if (!geometrySource.empty())
	{
		const uint32_t geometryShader = CompileStage(GL_GEOMETRY_SHADER, geometrySource, name + " [geometry]");
		if (geometryShader == 0)
		{
			for (uint32_t id : shaderIDs)
				glDeleteShader(id);
			return nullptr;
		}
		shaderIDs.push_back(geometryShader);
	}

	uint32_t program = LinkProgram(name, shaderIDs);
	if (program == 0)
		return nullptr;
	auto shader = Ref<GLShader>(new GLShader(program, name));
	shader->ReflectResourceBindingsFromSource(vertexSource);
	shader->ReflectResourceBindingsFromSource(fragmentSource);
	if (!geometrySource.empty())
		shader->ReflectResourceBindingsFromSource(geometrySource);
	return shader;
}

Ref<GLShader> GLShader::CreateFromFiles(
	const std::string &name,
	const std::string &vertexPath,
	const std::string &fragmentPath,
	const std::string &geometryPath)
{
	const auto vertexSource = FileSystem::ReadTextFile(vertexPath);
	if (!vertexSource)
	{
		LOG_ERROR_CAT(LogCategory::Shader, "Shader source load failed ({}): could not read vertex shader '{}'",
					  name, vertexPath);
		return nullptr;
	}

	const auto fragmentSource = FileSystem::ReadTextFile(fragmentPath);
	if (!fragmentSource)
	{
		LOG_ERROR_CAT(LogCategory::Shader, "Shader source load failed ({}): could not read fragment shader '{}'",
					  name, fragmentPath);
		return nullptr;
	}

	std::string geometrySource;
	if (!geometryPath.empty())
	{
		const auto geometryFileSource = FileSystem::ReadTextFile(geometryPath);
		if (!geometryFileSource)
		{
			LOG_ERROR_CAT(LogCategory::Shader, "Shader source load failed ({}): could not read geometry shader '{}'",
						  name, geometryPath);
			return nullptr;
		}
		geometrySource = *geometryFileSource;
	}

	return CreateFromSource(name, *vertexSource, *fragmentSource, geometrySource);
}

Ref<GLShader> GLShader::CreateFromCompiledGlsl(const std::string &name)
{
	auto baseDir = FileSystem::GetCompiledShaderDir() / "glsl";
	auto vertPath = baseDir / (name + ".vert.glsl");
	auto fragPath = baseDir / (name + ".frag.glsl");

	if (!FileSystem::Exists(vertPath))
	{
		LOG_ERROR_CAT(LogCategory::Shader,
					  "Compiled GLSL vertex shader missing: {}. Enable GLAB_COMPILE_SHADERS and rebuild, or build the CompileShaders target.",
					  vertPath.string());
		return nullptr;
	}
	if (!FileSystem::Exists(fragPath))
	{
		LOG_ERROR_CAT(LogCategory::Shader,
					  "Compiled GLSL fragment shader missing: {}. Enable GLAB_COMPILE_SHADERS and rebuild, or build the CompileShaders target.",
					  fragPath.string());
		return nullptr;
	}

	const auto vertSrc = FileSystem::ReadTextFile(vertPath);
	if (!vertSrc)
	{
		LOG_ERROR_CAT(LogCategory::Shader, "Compiled GLSL load failed ({}): could not read '{}'",
					  name, vertPath.string());
		return nullptr;
	}

	const auto fragSrc = FileSystem::ReadTextFile(fragPath);
	if (!fragSrc)
	{
		LOG_ERROR_CAT(LogCategory::Shader, "Compiled GLSL load failed ({}): could not read '{}'",
					  name, fragPath.string());
		return nullptr;
	}

	LOG_TRACE_CAT(LogCategory::Shader, "Slang-compiled vertex shader ({}):\n{}", name, *vertSrc);
	LOG_TRACE_CAT(LogCategory::Shader, "Slang-compiled fragment shader ({}):\n{}", name, *fragSrc);

	std::string geomSrc;
	auto geomPath = baseDir / (name + ".geom.glsl");
	if (FileSystem::Exists(geomPath))
	{
		const auto geometrySource = FileSystem::ReadTextFile(geomPath);
		if (!geometrySource)
		{
			LOG_ERROR_CAT(LogCategory::Shader, "Compiled GLSL load failed ({}): could not read '{}'",
						  name, geomPath.string());
			return nullptr;
		}
		geomSrc = *geometrySource;
	}

	auto shader = CreateFromSource(name, *vertSrc, *fragSrc, geomSrc);
	if (!shader)
		return nullptr;

	std::vector<std::filesystem::path> reflectionPaths = {
		baseDir / (name + ".vert.reflect.json"),
		baseDir / (name + ".frag.reflect.json")
	};
	const auto geomReflectPath = baseDir / (name + ".geom.reflect.json");
	if (FileSystem::Exists(geomReflectPath))
		reflectionPaths.push_back(geomReflectPath);

	shader->LoadSlangReflectionMetadata(reflectionPaths);
	return shader;
}

void GLShader::Bind() const
{
	glUseProgram(m_RendererID);
}

void GLShader::Unbind() const
{
	glUseProgram(0);
}

int GLShader::GetUniformLocation(const std::string &name)
{
	auto it = m_UniformLocationCache.find(name);
	if (it != m_UniformLocationCache.end())
		return it->second;

	int location = glGetUniformLocation(m_RendererID, name.c_str());
	m_UniformLocationCache[name] = location;
	return location;
}

void GLShader::SetInt(const std::string &name, int value)
{
	glProgramUniform1i(m_RendererID, GetUniformLocation(name), value);
}

void GLShader::SetIntArray(const std::string &name, const int *values, uint32_t count)
{
	glProgramUniform1iv(m_RendererID, GetUniformLocation(name), static_cast<GLsizei>(count), values);
}

void GLShader::SetBool(const std::string &name, bool value)
{
	glProgramUniform1i(m_RendererID, GetUniformLocation(name), value ? 1 : 0);
}

void GLShader::SetFloat(const std::string &name, float value)
{
	glProgramUniform1f(m_RendererID, GetUniformLocation(name), value);
}

void GLShader::SetFloat2(const std::string &name, const glm::vec2 &value)
{
	glProgramUniform2f(m_RendererID, GetUniformLocation(name), value.x, value.y);
}

void GLShader::SetFloat3(const std::string &name, const glm::vec3 &value)
{
	glProgramUniform3f(m_RendererID, GetUniformLocation(name), value.x, value.y, value.z);
}

void GLShader::SetFloat4(const std::string &name, const glm::vec4 &value)
{
	glProgramUniform4f(m_RendererID, GetUniformLocation(name), value.x, value.y, value.z, value.w);
}

void GLShader::SetMat3(const std::string &name, const glm::mat3 &value)
{
	glProgramUniformMatrix3fv(m_RendererID, GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}

void GLShader::SetMat4(const std::string &name, const glm::mat4 &value)
{
	glProgramUniformMatrix4fv(m_RendererID, GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}

void GLShader::SetUniformBlock(uint32_t binding, const void *data, uint32_t size)
{
	auto it = m_UBOCache.find(binding);
	if (it == m_UBOCache.end())
	{
		GLuint ubo = 0;
		glCreateBuffers(1, &ubo);
		glNamedBufferStorage(ubo, size, data, GL_DYNAMIC_STORAGE_BIT);
		glBindBufferBase(GL_UNIFORM_BUFFER, binding, ubo);
		m_UBOCache[binding] = ubo;
	}
	else
	{
		glNamedBufferSubData(it->second, 0, size, data);
		glBindBufferBase(GL_UNIFORM_BUFFER, binding, it->second);
	}
}

void GLShader::BindUniformBuffer(ShaderBindingPoint binding, const Ref<IUniformBuffer> &buffer)
{
	const uint32_t slot = ResolveGLBufferBindingIndex(m_BackendBindings, binding);
	if (!buffer)
	{
		glBindBufferBase(GL_UNIFORM_BUFFER, slot, 0);
		return;
	}

	auto *glBuffer = AsGL<GLUniformBuffer>(buffer);
	glBindBufferBase(GL_UNIFORM_BUFFER, slot, glBuffer->GetRendererID());
}

void GLShader::BindTexture(ShaderBindingPoint binding, const Ref<ITexture2D> &texture)
{
	const uint32_t slot = ResolveGLTextureBindingIndex(m_BackendBindings, binding);
	if (texture)
		texture->Bind(slot);
	else
		glBindTextureUnit(slot, 0);
}

const ShaderUniformBlockLayout *GLShader::GetUniformBlockLayout(ShaderBindingPoint binding) const
{
	auto it = m_BlockLayouts.find(binding);
	if (it == m_BlockLayouts.end())
		return nullptr;

	return &it->second;
}

const ShaderResourceLayout *GLShader::GetResourceLayout(ShaderBindingPoint binding) const
{
	auto it = m_ResourceLayouts.find(binding);
	if (it == m_ResourceLayouts.end())
		return nullptr;

	return &it->second;
}

const ShaderBackendBinding *GLShader::GetBackendBinding(ShaderBindingPoint binding) const
{
	auto it = m_BackendBindings.find(binding);
	if (it == m_BackendBindings.end())
		return nullptr;

	return &it->second;
}

void GLShader::ReflectUniformBlocks()
{
	GLint blockCount = 0;
	glGetProgramInterfaceiv(m_RendererID, GL_UNIFORM_BLOCK, GL_ACTIVE_RESOURCES, &blockCount);
	if (blockCount <= 0)
		return;

	const GLenum blockProps[] = {GL_BUFFER_BINDING, GL_BUFFER_DATA_SIZE, GL_NAME_LENGTH, GL_NUM_ACTIVE_VARIABLES};
	constexpr GLsizei blockPropCount = 4;

	for (GLint blockIndex = 0; blockIndex < blockCount; ++blockIndex)
	{
		GLint blockValues[blockPropCount] = {};
		glGetProgramResourceiv(m_RendererID, GL_UNIFORM_BLOCK, blockIndex,
		                       blockPropCount, blockProps, blockPropCount, nullptr, blockValues);

		const uint32_t binding = static_cast<uint32_t>(blockValues[0]);
		const uint32_t blockSize = static_cast<uint32_t>(blockValues[1]);
		const GLint blockNameLength = blockValues[2];
		const GLint uniformCount = blockValues[3];

		std::string blockName(blockNameLength > 0 ? blockNameLength : 1, '\0');
		glGetProgramResourceName(m_RendererID, GL_UNIFORM_BLOCK, blockIndex,
		                         static_cast<GLsizei>(blockName.size()), nullptr, blockName.data());

		ShaderUniformBlockLayout layout(TrimTrailingNull(std::move(blockName)),
		                               MakeFlatShaderBindingPoint(binding),
		                               blockSize);

		if (uniformCount > 0)
		{
			std::vector<GLint> uniformIndices(static_cast<size_t>(uniformCount), -1);
			const GLenum activeVariablesProp = GL_ACTIVE_VARIABLES;
			glGetProgramResourceiv(m_RendererID, GL_UNIFORM_BLOCK, blockIndex,
			                       1, &activeVariablesProp, uniformCount, nullptr, uniformIndices.data());

			const GLenum uniformProps[] = {GL_OFFSET, GL_TYPE, GL_NAME_LENGTH, GL_ARRAY_SIZE};
			constexpr GLsizei uniformPropCount = 4;

			for (GLint uniformIndex : uniformIndices)
			{
				if (uniformIndex < 0)
					continue;

				GLint uniformValues[uniformPropCount] = {};
				glGetProgramResourceiv(m_RendererID, GL_UNIFORM, uniformIndex,
				                       uniformPropCount, uniformProps, uniformPropCount, nullptr, uniformValues);

				std::string uniformName(uniformValues[2] > 0 ? uniformValues[2] : 1, '\0');
				glGetProgramResourceName(m_RendererID, GL_UNIFORM, uniformIndex,
				                         static_cast<GLsizei>(uniformName.size()), nullptr, uniformName.data());

				const ShaderUniformValueType type = MapGLUniformType(static_cast<GLenum>(uniformValues[1]));
				uint32_t fieldSize = GetShaderUniformValueTypeSize(type);
				const uint32_t arraySize = static_cast<uint32_t>(uniformValues[3]);
				if (arraySize > 1)
					fieldSize *= arraySize;

				layout.AddField({
					NormalizeGLUniformFieldName(TrimTrailingNull(std::move(uniformName))),
					static_cast<uint32_t>(uniformValues[0]),
					fieldSize,
					type
				});
			}
		}

		LOG_TRACE_CAT(LogCategory::Shader, "GLShader '{}': reflected uniform block '{}' at binding {} ({} bytes, {} fields)",
		              m_Name, layout.GetName(), binding, layout.GetSize(), layout.GetFields().size());
		const ShaderBindingPoint logicalBinding = layout.GetBindingPoint();
		m_ResourceLayouts[logicalBinding] = {
			layout.GetName(),
			ShaderResourceKind::UniformBuffer,
			logicalBinding
		};
		m_BackendBindings[logicalBinding] = {
			ShaderResourceKind::UniformBuffer,
			logicalBinding,
			binding,
			std::nullopt,
			std::nullopt
		};
		m_BlockLayouts[logicalBinding] = std::move(layout);
	}
}

void GLShader::ReflectResourceBindingsFromSource(std::string_view source)
{
	static const std::regex kSamplerBindingPattern(
		R"(layout\s*\(\s*binding\s*=\s*(\d+)\s*\)\s*uniform\s+([A-Za-z_][A-Za-z0-9_]*)\s+([A-Za-z_][A-Za-z0-9_]*)\s*;)");

	const std::string sourceText(source);
	for (std::sregex_iterator it(sourceText.begin(), sourceText.end(), kSamplerBindingPattern), end; it != end; ++it)
	{
		const std::smatch &match = *it;
		if (match.size() < 4)
			continue;

		const std::string typeName = match[2].str();
		if (typeName.rfind("sampler", 0) != 0)
			continue;

		const uint32_t bindingIndex = static_cast<uint32_t>(std::stoul(match[1].str()));
		const ShaderBindingPoint logicalBinding = MakeFlatShaderBindingPoint(bindingIndex);
		const std::string logicalName = NormalizeGLUniformFieldName(match[3].str());

		m_ResourceLayouts[logicalBinding] = {
			logicalName,
			ShaderResourceKind::CombinedTextureSampler,
			logicalBinding
		};
		m_BackendBindings[logicalBinding] = {
			ShaderResourceKind::CombinedTextureSampler,
			logicalBinding,
			std::nullopt,
			bindingIndex,
			bindingIndex
		};
	}
}

bool GLShader::LoadSlangReflectionMetadata(const std::vector<std::filesystem::path> &reflectionPaths)
{
	std::unordered_map<ShaderBindingPoint, ShaderUniformBlockLayout, ShaderBindingPointHash> blockLayouts;
	ShaderResourceLayoutMap resourceLayouts;
	ShaderBackendBindingMap backendBindings;
	bool loadedAny = false;

	for (const auto &path : reflectionPaths)
	{
		if (!FileSystem::Exists(path))
			continue;

		const auto reflectionText = FileSystem::ReadTextFile(path);
		if (!reflectionText)
		{
			LOG_WARN_CAT(LogCategory::Shader, "GLShader '{}': failed to read reflection sidecar '{}'",
			             m_Name, path.string());
			continue;
		}

		try
		{
			const auto json = nlohmann::json::parse(*reflectionText);
			if (!json.contains("parameters") || !json["parameters"].is_array())
				continue;

			loadedAny = true;
			for (const auto &parameter : json["parameters"])
			{
				if (!parameter.contains("name") || !parameter.contains("binding"))
					continue;

				const auto logicalBinding = ParseLogicalBindingPoint(parameter["binding"]);
				if (!logicalBinding.has_value())
					continue;

				const auto &type = parameter.value("type", nlohmann::json::object());
				const std::string typeKind = type.value("kind", std::string{});
				const uint32_t flatBinding = FlattenShaderBindingPointForOpenGL(*logicalBinding);
				const std::string resourceName = parameter.value("name", std::string{});

				if (typeKind == "constantBuffer")
				{
					ShaderUniformBlockLayout layout(resourceName, *logicalBinding, 0u);
					AppendReflectedStructFields(type, layout);
					layout.SetSize(std::max(parameter["binding"].value("size", 0u),
					                        ComputeReflectedLayoutSize(layout)));

					resourceLayouts[*logicalBinding] = {
						resourceName,
						ShaderResourceKind::UniformBuffer,
						*logicalBinding
					};
					backendBindings[*logicalBinding] = {
						ShaderResourceKind::UniformBuffer,
						*logicalBinding,
						flatBinding,
						std::nullopt,
						std::nullopt
					};
					blockLayouts[*logicalBinding] = std::move(layout);
					continue;
				}

				if (typeKind == "resource")
				{
					resourceLayouts[*logicalBinding] = {
						resourceName,
						ShaderResourceKind::CombinedTextureSampler,
						*logicalBinding
					};
					backendBindings[*logicalBinding] = {
						ShaderResourceKind::CombinedTextureSampler,
						*logicalBinding,
						std::nullopt,
						flatBinding,
						flatBinding
					};
				}
			}
		}
		catch (const std::exception &e)
		{
			LOG_WARN_CAT(LogCategory::Shader, "GLShader '{}': failed to parse reflection sidecar '{}': {}",
			             m_Name, path.string(), e.what());
		}
	}

	if (!loadedAny)
		return false;

	if (!blockLayouts.empty())
		m_BlockLayouts = std::move(blockLayouts);
	if (!resourceLayouts.empty())
		m_ResourceLayouts = std::move(resourceLayouts);
	if (!backendBindings.empty())
		m_BackendBindings = std::move(backendBindings);

	LOG_TRACE_CAT(LogCategory::Shader, "GLShader '{}': loaded Slang reflection metadata from {} sidecar(s)",
	              m_Name, reflectionPaths.size());
	return true;
}
