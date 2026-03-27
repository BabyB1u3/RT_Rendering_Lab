#include "GLShader.h"

#include <array>
#include <vector>

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#include "core/FileSystem.h"
#include "core/diagnostics/LogCategories.h"
#include "core/diagnostics/LogMacros.h"

GLShader::GLShader(uint32_t program, std::string name)
	: m_RendererID(program), m_Name(std::move(name))
{
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
	  m_UBOCache(std::move(other.m_UBOCache))
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
	return Ref<GLShader>(new GLShader(program, name));
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

	return CreateFromSource(name, *vertSrc, *fragSrc, geomSrc);
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
