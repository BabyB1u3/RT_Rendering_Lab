#pragma once

/// @file MetalImGuiBridge.h
/// @brief Objective-C++ bridge for Dear ImGui's Metal backend.
///
/// Keeps Metal/Obj-C details out of ImGuiLayer.cpp by exposing a tiny C++
/// surface with the same lifecycle shape used by the higher-level ImGui layer:
/// Init -> NewFrame -> RenderDrawData -> Shutdown.

namespace MetalImGuiBridge
{
void Init(void* mtlDevice);
void Shutdown();
void NewFrame(void* drawableTexture);
void RenderDrawData(void* drawData, void* commandBuffer, void* drawableTexture);
} // namespace MetalImGuiBridge
