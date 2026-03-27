#pragma once

/// @file IShader.h
/// @brief Abstract interface for shader programs. Backend-agnostic.

#include <cstdint>
#include <string>

#include <glm/glm.hpp>

class IShader
{
public:
	virtual ~IShader() = default;

	virtual void Bind() const = 0;
	virtual void Unbind() const = 0;

	virtual const std::string &GetName() const = 0;

	virtual void SetInt(const std::string &name, int value) = 0;
	virtual void SetIntArray(const std::string &name, const int *values, uint32_t count) = 0;
	virtual void SetBool(const std::string &name, bool value) = 0;
	virtual void SetFloat(const std::string &name, float value) = 0;
	virtual void SetFloat2(const std::string &name, const glm::vec2 &value) = 0;
	virtual void SetFloat3(const std::string &name, const glm::vec3 &value) = 0;
	virtual void SetFloat4(const std::string &name, const glm::vec4 &value) = 0;
	virtual void SetMat3(const std::string &name, const glm::mat3 &value) = 0;
	virtual void SetMat4(const std::string &name, const glm::mat4 &value) = 0;

	/// Upload data to a uniform block at the specified binding point.
	/// Maps to UBO on OpenGL, descriptor set buffer on Vulkan.
	virtual void SetUniformBlock(uint32_t binding, const void *data, uint32_t size) = 0;
};
