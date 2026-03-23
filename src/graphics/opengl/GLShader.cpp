#include "GLShader.h"

#include <array>
#include <cstring>
#include <stdexcept>
#include <vector>

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <spirv_glsl.hpp>

#include "core/FileSystem.h"
#include "core/Logger.h"

GLShader::GLShader(uint32_t program, std::string name)
	: m_RendererID(program), m_Name(std::move(name))
{
}

GLShader::~GLShader()
{
	if (m_RendererID != 0)
		glDeleteProgram(m_RendererID);
}

GLShader::GLShader(GLShader &&other) noexcept
	: m_RendererID(other.m_RendererID),
	  m_Name(std::move(other.m_Name)),
	  m_UniformLocationCache(std::move(other.m_UniformLocationCache))
{
	other.m_RendererID = 0;
}

GLShader &GLShader::operator=(GLShader &&other) noexcept
{
	if (this == &other)
		return *this;

	if (m_RendererID != 0)
		glDeleteProgram(m_RendererID);

	m_RendererID = other.m_RendererID;
	m_Name = std::move(other.m_Name);
	m_UniformLocationCache = std::move(other.m_UniformLocationCache);

	other.m_RendererID = 0;
	return *this;
}

std::string GLShader::TranspileSpirvToGlsl(const std::vector<uint8_t> &spirvBytes)
{
	if (spirvBytes.size() % 4 != 0)
		throw std::runtime_error("SPIR-V binary size is not a multiple of 4 bytes");

	std::vector<uint32_t> words(spirvBytes.size() / 4);
	std::memcpy(words.data(), spirvBytes.data(), spirvBytes.size());

	spirv_cross::CompilerGLSL compiler(std::move(words));

	spirv_cross::CompilerGLSL::Options opts;
	opts.version = 460;
	opts.es = false;
	opts.vulkan_semantics = false;
	opts.enable_420pack_extension = true;
	compiler.set_common_options(opts);

	return compiler.compile();
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

		LOG_ERROR("Shader compilation failed ({}): {}", debugName, infoLog.data());
		throw std::runtime_error("Shader compilation failed (" + debugName + "):\n" + std::string(infoLog.data()));
	}

	LOG_TRACE("Shader stage compiled: {}", debugName);
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

		LOG_ERROR("Shader link failed ({}): {}", name, infoLog.data());
		throw std::runtime_error("Shader link failed (" + name + "):\n" + std::string(infoLog.data()));
	}

	for (uint32_t id : shaderIDs)
	{
		glDetachShader(program, id);
		glDeleteShader(id);
	}

	LOG_INFO("Shader linked: {}", name);
	return program;
}

Ref<GLShader> GLShader::CreateFromSource(
	const std::string &name,
	const std::string &vertexSource,
	const std::string &fragmentSource,
	const std::string &geometrySource)
{
	std::vector<uint32_t> shaderIDs;
	shaderIDs.push_back(CompileStage(GL_VERTEX_SHADER, vertexSource, name + " [vertex]"));
	shaderIDs.push_back(CompileStage(GL_FRAGMENT_SHADER, fragmentSource, name + " [fragment]"));

	if (!geometrySource.empty())
		shaderIDs.push_back(CompileStage(GL_GEOMETRY_SHADER, geometrySource, name + " [geometry]"));

	uint32_t program = LinkProgram(name, shaderIDs);
	return Ref<GLShader>(new GLShader(program, name));
}

Ref<GLShader> GLShader::CreateFromFiles(
	const std::string &name,
	const std::string &vertexPath,
	const std::string &fragmentPath,
	const std::string &geometryPath)
{
	const std::string vertexSource = FileSystem::ReadTextFile(vertexPath);
	const std::string fragmentSource = FileSystem::ReadTextFile(fragmentPath);
	const std::string geometrySource = geometryPath.empty() ? "" : FileSystem::ReadTextFile(geometryPath);

	return CreateFromSource(name, vertexSource, fragmentSource, geometrySource);
}

Ref<GLShader> GLShader::CreateFromStem(const std::filesystem::path &stemPath, const std::string &name)
{
	auto vertSpvPath = stemPath.string() + ".vert.spv";
	auto fragSpvPath = stemPath.string() + ".frag.spv";

	if (!FileSystem::Exists(vertSpvPath))
		throw std::runtime_error(
			"Shader artifact missing: " + vertSpvPath +
			"\nEnable GLAB_COMPILE_SHADERS and rebuild, or build the CompileShaders target.");
	if (!FileSystem::Exists(fragSpvPath))
		throw std::runtime_error(
			"Shader artifact missing: " + fragSpvPath +
			"\nEnable GLAB_COMPILE_SHADERS and rebuild, or build the CompileShaders target.");

	auto vertSpv = FileSystem::ReadBinaryFile(vertSpvPath);
	auto fragSpv = FileSystem::ReadBinaryFile(fragSpvPath);

	std::string vertGlsl = TranspileSpirvToGlsl(vertSpv);
	std::string fragGlsl = TranspileSpirvToGlsl(fragSpv);

	LOG_TRACE("SPIRV-Cross transpiled vertex shader ({}):\n{}", stemPath.stem().string(), vertGlsl);
	LOG_TRACE("SPIRV-Cross transpiled fragment shader ({}):\n{}", stemPath.stem().string(), fragGlsl);

	std::string geomGlsl;
	auto geomSpvPath = stemPath.string() + ".geom.spv";
	if (FileSystem::Exists(geomSpvPath))
	{
		auto geomSpv = FileSystem::ReadBinaryFile(geomSpvPath);
		geomGlsl = TranspileSpirvToGlsl(geomSpv);
	}

	std::string shaderName = name.empty() ? stemPath.stem().string() : name;
	return CreateFromSource(shaderName, vertGlsl, fragGlsl, geomGlsl);
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
