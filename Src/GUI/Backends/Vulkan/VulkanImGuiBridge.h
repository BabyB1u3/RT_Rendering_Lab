#pragma once

/// @file VulkanImGuiBridge.h
/// @brief C++ bridge for Dear ImGui's Vulkan backend.

class CommandList;
class Device;
class Swapchain;
class TextureView;

namespace VulkanImGuiBridge
{
void Init(Device& device, Swapchain& swapchain);
void Shutdown();
void NewFrame();
void RenderDrawData(void* drawData, CommandList* commandList, TextureView* targetView);
} // namespace VulkanImGuiBridge
