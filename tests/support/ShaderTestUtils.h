#pragma once

#include <filesystem>
#include <string>

#include <gtest/gtest.h>

#include "core/FileSystem.h"

namespace ShaderTestUtils
{
	inline std::filesystem::path GetCompiledGlslPath(const std::string &shaderName, const std::string &stage)
	{
		return FileSystem::GetCompiledShaderDir() / "glsl" / (shaderName + "." + stage + ".glsl");
	}

	inline bool HasCompiledGlslArtifacts(const char *shaderName)
	{
		return FileSystem::Exists(GetCompiledGlslPath(shaderName, "vert")) &&
			   FileSystem::Exists(GetCompiledGlslPath(shaderName, "frag"));
	}

	inline bool HasAllRequiredShaders()
	{
		return HasCompiledGlslArtifacts("ForwardLit") &&
			   HasCompiledGlslArtifacts("ShadowDepth") &&
			   HasCompiledGlslArtifacts("TexturePreview");
	}

	/// When GLAB_REQUIRE_COMPILED_SHADERS is set (e.g. in CI), missing shaders
	/// cause a hard FAIL instead of a silent GTEST_SKIP. This prevents CI from
	/// passing with zero shader-dependent tests actually running.
	inline void SkipOrFailIfShadersMissing(const char *context = "")
	{
		if (HasAllRequiredShaders())
			return;

		const char *require = std::getenv("GLAB_REQUIRE_COMPILED_SHADERS");
		if (require && std::string(require) != "0" && std::string(require) != "false")
		{
			FAIL() << "Compiled GLSL artifacts not found and GLAB_REQUIRE_COMPILED_SHADERS is set. "
				   << context;
		}
		else
		{
			GTEST_SKIP() << "Compiled GLSL artifacts not found. " << context;
		}
	}

	inline void SkipOrFailIfShaderMissing(const char *shaderName, const char *context = "")
	{
		if (HasCompiledGlslArtifacts(shaderName))
			return;

		const char *require = std::getenv("GLAB_REQUIRE_COMPILED_SHADERS");
		if (require && std::string(require) != "0" && std::string(require) != "false")
		{
			FAIL() << "Compiled GLSL artifacts for '" << shaderName
				   << "' not found and GLAB_REQUIRE_COMPILED_SHADERS is set. " << context;
		}
		else
		{
			GTEST_SKIP() << "Compiled GLSL artifacts for '" << shaderName
						 << "' not found. " << context;
		}
	}
}
