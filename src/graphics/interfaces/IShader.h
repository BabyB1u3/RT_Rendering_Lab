#pragma once

/// @file IShader.h
/// @brief Abstract interface for shader programs. Backend-agnostic.

#include <cstdint>
#include <string>

#include <glm/glm.hpp>

#include "core/Base.h"
#include "graphics/ShaderUniformLayout.h"

class ITexture2D;
class IUniformBuffer;

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

	/// Bind a logical uniform buffer object at the specified slot.
	///
	/// Cross-backend contract:
	/// - Call sites may reuse one IUniformBuffer across many draw calls in the same
	///   pass and update it with SetData() between draws.
	/// - Backends must preserve per-draw stability. A later SetData() must not
	///   retroactively change the bytes consumed by previously encoded draws.
	///
	/// This requirement became explicit after a Metal regression where directly
	/// binding one mutable MTLBuffer caused every draw in the pass to observe the
	/// final upload instead of the upload current at that draw.
	virtual void BindUniformBuffer(ShaderBindingPoint binding, const Ref<IUniformBuffer> &buffer) = 0;

	/// Compatibility shim for bridge code that still uses flat slots.
	void BindUniformBuffer(uint32_t slot, const Ref<IUniformBuffer> &buffer)
	{
		BindUniformBuffer(MakeFlatShaderBindingPoint(slot), buffer);
	}

	/// Bind a texture at the specified slot.
	/// OpenGL binding is stage-agnostic. Metal currently binds the texture to both
	/// vertex and fragment stages to preserve a simple slot-only abstraction.
	virtual void BindTexture(ShaderBindingPoint binding, const Ref<ITexture2D> &texture) = 0;

	/// Compatibility shim for bridge code that still uses flat slots.
	void BindTexture(uint32_t slot, const Ref<ITexture2D> &texture)
	{
		BindTexture(MakeFlatShaderBindingPoint(slot), texture);
	}

	/// Returns the authoritative reflected layout for the requested uniform block,
	/// or nullptr when the backend has no layout metadata for that binding yet.
	virtual const ShaderUniformBlockLayout *GetUniformBlockLayout(ShaderBindingPoint binding) const = 0;

	/// Compatibility shim for bridge code that still uses flat slots.
	const ShaderUniformBlockLayout *GetUniformBlockLayout(uint32_t binding) const
	{
		return GetUniformBlockLayout(MakeFlatShaderBindingPoint(binding));
	}
};
