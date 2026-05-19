#pragma once

/// @file MetalImGuiBridge.h
/// @brief Objective-C++ bridge for Dear ImGui's Metal backend.
///
/// Keeps Metal/Obj-C details out of ImGuiLayer.cpp by exposing a tiny C++
/// surface with the same lifecycle shape used by the higher-level ImGui layer:
/// Init -> NewFrame -> RenderDrawData -> Shutdown.

class Device;
class Texture;

namespace MetalImGuiBridge
{
void Init(Device& device);
void Shutdown();
void NewFrame(Texture* drawableTexture);
void RenderDrawData(void* drawData, Device& device, Texture* drawableTexture);
} // namespace MetalImGuiBridge
