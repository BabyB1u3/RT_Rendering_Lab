#pragma once

/// @file MetalCommon.h
/// @brief Objective-C Metal imports plus backend-private PIMPL data structs.
/// This header is intended for Objective-C++ translation units and resource
/// wrappers, not for generic C++ callers of the Metal backend device API.

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

struct MetalDeviceData
{
    id<MTLDevice> m_Device = nil;
    id<MTLCommandQueue> m_CommandQueue = nil;
    id<MTLCommandBuffer> m_CurrentCommandBuffer = nil;
    NSAutoreleasePool* m_FrameAutoreleasePool = nil;
    bool m_FrameInProgress = false;
    bool m_FrameSubmitted = false;
};

struct MetalCommandListData
{
    MetalDeviceData* m_DeviceData = nullptr;
    id<MTLRenderCommandEncoder> m_RenderEncoder = nil;
};

struct MetalSwapchainData
{
    CAMetalLayer* m_Layer = nil;
    id<CAMetalDrawable> m_CurrentDrawable = nil;
};
