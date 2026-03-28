#pragma once

/// @file GLShader.h
/// @brief OpenGL shader program implementation of IShader with uniform caching.

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "core/Base.h"
#include "graphics/ShaderUniformLayout.h"
#include "graphics/interfaces/IShader.h"

class GLShader : public IShader
{
public:
	GLShader() = default;
	~GLShader() override;

	GLShader(const GLShader &) = delete;
	GLShader &operator=(const GLShader &) = delete;

	GLShader(GLShader &&other) noexcept;
	GLShader &operator=(GLShader &&other) noexcept;

	// --- IShader interface ---
	void Bind() const override;
	void Unbind() const override;

	const std::string &GetName() const override { return m_Name; }

	void SetInt(const std::string &name, int value) override;
	void SetIntArray(const std::string &name, const int *values, uint32_t count) override;
	void SetBool(const std::string &name, bool value) override;
	void SetFloat(const std::string &name, float value) override;
	void SetFloat2(const std::string &name, const glm::vec2 &value) override;
	void SetFloat3(const std::string &name, const glm::vec3 &value) override;
	void SetFloat4(const std::string &name, const glm::vec4 &value) override;
	void SetMat3(const std::string &name, const glm::mat3 &value) override;
	void SetMat4(const std::string &name, const glm::mat4 &value) override;
	void SetUniformBlock(uint32_t binding, const void *data, uint32_t size) override;
	void BindUniformBuffer(uint32_t slot, const Ref<IUniformBuffer> &buffer) override;
	void BindTexture(uint32_t slot, const Ref<ITexture2D> &texture) override;
	const ShaderUniformBlockLayout *GetUniformBlockLayout(uint32_t binding) const override;

	// --- GL-specific (non-virtual) ---
	uint32_t GetRendererID() const { return m_RendererID; }

	// Factory methods (used by GLGraphicsDevice)
	static Ref<GLShader> CreateFromSource(
		const std::string &name,
		const std::string &vertexSource,
		const std::string &fragmentSource,
		const std::string &geometrySource = "");

	static Ref<GLShader> CreateFromFiles(
		const std::string &name,
		const std::string &vertexPath,
		const std::string &fragmentPath,
		const std::string &geometryPath = "");

	/// Load pre-compiled GLSL produced by slangc from the build output directory.
	static Ref<GLShader> CreateFromCompiledGlsl(const std::string &name);

private:
	GLShader(uint32_t program, std::string name);

	static uint32_t CompileStage(uint32_t stage, const std::string &source, const std::string &debugName);
	static uint32_t LinkProgram(const std::string &name, const std::vector<uint32_t> &shaderIDs);
	void ReflectUniformBlocks();

	int GetUniformLocation(const std::string &name);

private:
	uint32_t m_RendererID = 0;
	std::string m_Name;
	std::unordered_map<std::string, int> m_UniformLocationCache;
	std::unordered_map<uint32_t, uint32_t> m_UBOCache;
	std::unordered_map<uint32_t, ShaderUniformBlockLayout> m_BlockLayouts;
};
