#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include <glm/glm.hpp>

class Shader
{
public:
	Shader() = default;
	~Shader();

	Shader(const Shader &) = delete;
	Shader &operator=(const Shader &) = delete;

	Shader(Shader &&other) noexcept;
	Shader &operator=(Shader &&other) noexcept;

	static std::shared_ptr<Shader> CreateFromSource(
		const std::string &name,
		const std::string &vertexSource,
		const std::string &fragmentSource,
		const std::string &geometrySource = "");

	static std::shared_ptr<Shader> CreateFromFiles(
		const std::string &name,
		const std::string &vertexPath,
		const std::string &fragmentPath,
		const std::string &geometryPath = "");

	static std::shared_ptr<Shader> CreateFromSingleFile(
		const std::string &filepath,
		const std::string &name = "");

	void Bind() const;
	void Unbind() const;

	const std::string &GetName() const { return m_Name; }
	uint32_t GetRendererID() const { return m_RendererID; }

	void SetInt(const std::string &name, int value);
	void SetIntArray(const std::string &name, const int *values, uint32_t count);
	void SetBool(const std::string &name, bool value);
	void SetFloat(const std::string &name, float value);
	void SetFloat2(const std::string &name, const glm::vec2 &value);
	void SetFloat3(const std::string &name, const glm::vec3 &value);
	void SetFloat4(const std::string &name, const glm::vec4 &value);
	void SetMat3(const std::string &name, const glm::mat3 &value);
	void SetMat4(const std::string &name, const glm::mat4 &value);

private:
	Shader(uint32_t program, std::string name);

	static std::string ReadTextFile(const std::string &filepath);
	static std::unordered_map<uint32_t, std::string> PreProcessSingleFile(const std::string &source);

	static uint32_t CompileStage(uint32_t stage, const std::string &source, const std::string &debugName);
	static uint32_t LinkProgram(const std::string &name, const std::vector<uint32_t> &shaderIDs);

	int GetUniformLocation(const std::string &name);

private:
	uint32_t m_RendererID = 0;
	std::string m_Name;
	std::unordered_map<std::string, int> m_UniformLocationCache;
};