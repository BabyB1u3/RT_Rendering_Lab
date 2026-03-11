#include "Shader.h"

#include <array>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

static GLenum ShaderTypeFromString(const std::string &type)
{
	if (type == "vertex")
		return GL_VERTEX_SHADER;
	if (type == "fragment")
		return GL_FRAGMENT_SHADER;
	if (type == "geometry")
		return GL_GEOMETRY_SHADER;
	return 0;
}

Shader::Shader(uint32_t program, std::string name)
	: m_RendererID(program), m_Name(std::move(name))
{
}

Shader::~Shader()
{
	if (m_RendererID != 0)
		glDeleteProgram(m_RendererID);
}

Shader::Shader(Shader &&other) noexcept
	: m_RendererID(other.m_RendererID),
	  m_Name(std::move(other.m_Name)),
	  m_UniformLocationCache(std::move(other.m_UniformLocationCache))
{
	other.m_RendererID = 0;
}

Shader &Shader::operator=(Shader &&other) noexcept
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

std::string Shader::ReadTextFile(const std::string &filepath)
{
	std::ifstream in(filepath, std::ios::in | std::ios::binary);
	if (!in)
		throw std::runtime_error("Failed to open shader file: " + filepath);

	std::ostringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

std::unordered_map<uint32_t, std::string> Shader::PreProcessSingleFile(const std::string &source)
{
	std::unordered_map<uint32_t, std::string> shaderSources;

	const char *typeToken = "#type";
	const size_t typeTokenLength = std::strlen(typeToken);
	size_t pos = source.find(typeToken, 0);

	while (pos != std::string::npos)
	{
		size_t eol = source.find_first_of("\r\n", pos);
		if (eol == std::string::npos)
			throw std::runtime_error("Shader syntax error: missing newline after #type directive");

		size_t begin = pos + typeTokenLength + 1;
		std::string type = source.substr(begin, eol - begin);
		GLenum shaderType = ShaderTypeFromString(type);
		if (shaderType == 0)
			throw std::runtime_error("Shader syntax error: unknown shader type '" + type + "'");

		size_t nextLinePos = source.find_first_not_of("\r\n", eol);
		pos = source.find(typeToken, nextLinePos);

		shaderSources[shaderType] = source.substr(
			nextLinePos,
			pos == std::string::npos ? std::string::npos : pos - nextLinePos);
	}

	return shaderSources;
}

uint32_t Shader::CompileStage(uint32_t stage, const std::string &source, const std::string &debugName)
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

		throw std::runtime_error("Shader compilation failed (" + debugName + "):\n" + std::string(infoLog.data()));
	}

	return shader;
}

uint32_t Shader::LinkProgram(const std::string &name, const std::vector<uint32_t> &shaderIDs)
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

		throw std::runtime_error("Shader link failed (" + name + "):\n" + std::string(infoLog.data()));
	}

	for (uint32_t id : shaderIDs)
	{
		glDetachShader(program, id);
		glDeleteShader(id);
	}

	return program;
}

Ref<Shader> Shader::CreateFromSource(
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
	return Ref<Shader>(new Shader(program, name));
}

Ref<Shader> Shader::CreateFromFiles(
	const std::string &name,
	const std::string &vertexPath,
	const std::string &fragmentPath,
	const std::string &geometryPath)
{
	const std::string vertexSource = ReadTextFile(vertexPath);
	const std::string fragmentSource = ReadTextFile(fragmentPath);
	const std::string geometrySource = geometryPath.empty() ? "" : ReadTextFile(geometryPath);

	return CreateFromSource(name, vertexSource, fragmentSource, geometrySource);
}

Ref<Shader> Shader::CreateFromSingleFile(const std::string &filepath, const std::string &name)
{
	std::string source = ReadTextFile(filepath);
	auto sources = PreProcessSingleFile(source);

	if (!sources.count(GL_VERTEX_SHADER))
		throw std::runtime_error("Shader file '" + filepath + "' is missing a #type vertex section");
	if (!sources.count(GL_FRAGMENT_SHADER))
		throw std::runtime_error("Shader file '" + filepath + "' is missing a #type fragment section");

	std::string shaderName = name.empty() ? filepath : name;
	std::string geometrySource = sources.count(GL_GEOMETRY_SHADER) ? sources[GL_GEOMETRY_SHADER] : "";

	return CreateFromSource(
		shaderName,
		sources[GL_VERTEX_SHADER],
		sources[GL_FRAGMENT_SHADER],
		geometrySource);
}

void Shader::Bind() const
{
	glUseProgram(m_RendererID);
}

void Shader::Unbind() const
{
	glUseProgram(0);
}

int Shader::GetUniformLocation(const std::string &name)
{
	auto it = m_UniformLocationCache.find(name);
	if (it != m_UniformLocationCache.end())
		return it->second;

	int location = glGetUniformLocation(m_RendererID, name.c_str());
	m_UniformLocationCache[name] = location;
	return location;
}

void Shader::SetInt(const std::string &name, int value)
{
	glProgramUniform1i(m_RendererID, GetUniformLocation(name), value);
}

void Shader::SetIntArray(const std::string &name, const int *values, uint32_t count)
{
	glProgramUniform1iv(m_RendererID, GetUniformLocation(name), static_cast<GLsizei>(count), values);
}

void Shader::SetBool(const std::string &name, bool value)
{
	glProgramUniform1i(m_RendererID, GetUniformLocation(name), value ? 1 : 0);
}

void Shader::SetFloat(const std::string &name, float value)
{
	glProgramUniform1f(m_RendererID, GetUniformLocation(name), value);
}

void Shader::SetFloat2(const std::string &name, const glm::vec2 &value)
{
	glProgramUniform2f(m_RendererID, GetUniformLocation(name), value.x, value.y);
}

void Shader::SetFloat3(const std::string &name, const glm::vec3 &value)
{
	glProgramUniform3f(m_RendererID, GetUniformLocation(name), value.x, value.y, value.z);
}

void Shader::SetFloat4(const std::string &name, const glm::vec4 &value)
{
	glProgramUniform4f(m_RendererID, GetUniformLocation(name), value.x, value.y, value.z, value.w);
}

void Shader::SetMat3(const std::string &name, const glm::mat3 &value)
{
	glProgramUniformMatrix3fv(m_RendererID, GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::SetMat4(const std::string &name, const glm::mat4 &value)
{
	glProgramUniformMatrix4fv(m_RendererID, GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}