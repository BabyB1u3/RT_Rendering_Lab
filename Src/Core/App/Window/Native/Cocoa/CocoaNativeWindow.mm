#include "Core/App/Window/Native/Cocoa/CocoaNativeWindow.h"

#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>

NativeWindowHandle CreateCocoaNativeWindowHandle(GLFWwindow* window)
{
    NativeWindowHandle nativeWindowHandle{};
    nativeWindowHandle.m_System = NativeWindowSystem::Cocoa;

    NSWindow* nsWindow = glfwGetCocoaWindow(window);
    NSView* nsView = glfwGetCocoaView(window);

    nativeWindowHandle.m_Window = reinterpret_cast<uintptr_t>((__bridge void*)nsWindow);

#if defined(GLAB_BACKEND_METAL) || defined(GLAB_BACKEND_VULKAN)
    // Layer creation is best-effort here. If GLFW/Cocoa does not currently expose
    // a valid NSView, the returned layer may remain null and the RHI consumer must
    // validate the handle before attempting swapchain creation.
    // The returned layer pointer is borrowed through NativeWindowHandle; backend code that
    // stores it beyond the call site must retain/own it explicitly on its side.
    CAMetalLayer* layer = nil;

    if (nsView != nil)
    {
        if ([nsView.layer isKindOfClass:[CAMetalLayer class]])
        {
            layer = static_cast<CAMetalLayer*>(nsView.layer);
        }
        else
        {
            layer = [CAMetalLayer layer];
            nsView.wantsLayer = YES;
            nsView.layer = layer;
        }

        const CGFloat scale = nsWindow != nil ? nsWindow.backingScaleFactor : 1.0;
        layer.contentsScale = scale;

        CGSize drawableSize = nsView.bounds.size;
        drawableSize.width *= scale;
        drawableSize.height *= scale;
        layer.drawableSize = drawableSize;
    }

    nativeWindowHandle.m_Layer = (__bridge void*)layer;
#endif

    return nativeWindowHandle;
}
