#include "Core/App/Window/Native/Cocoa/CocoaNativeWindow.h"
#include "Core/App/Window/Window.h"

#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>
#import <objc/runtime.h>

static char kCocoaLiveResizeObserverKey;

@interface RTRLabCocoaWindowObserver : NSObject
{
@private
    GLFWwindow* _glfwWindow;
}

- (instancetype)initWithGLFWWindow:(GLFWwindow*)window;
- (void)requestRefresh;
- (void)syncLayerMetrics;
- (void)handleWindowDidResize:(NSNotification*)notification;
- (void)handleWindowDidChangeBackingProperties:(NSNotification*)notification;
- (void)startObservingWindow:(NSWindow*)window;
- (void)stopObservingWindow:(NSWindow*)window;

@end

@implementation RTRLabCocoaWindowObserver

- (instancetype)initWithGLFWWindow:(GLFWwindow*)window
{
    self = [super init];
    if (self != nil)
        _glfwWindow = window;
    return self;
}

- (void)requestRefresh
{
    auto* runtimeWindow = static_cast<Window*>(glfwGetWindowUserPointer(_glfwWindow));
    if (runtimeWindow != nullptr)
        runtimeWindow->InvokeRefreshCallback();
}

- (void)syncLayerMetrics
{
    NSWindow* nsWindow = glfwGetCocoaWindow(_glfwWindow);
    NSView* nsView = glfwGetCocoaView(_glfwWindow);
    if (nsWindow == nil || nsView == nil || ![nsView.layer isKindOfClass:[CAMetalLayer class]])
        return;

    CAMetalLayer* layer = static_cast<CAMetalLayer*>(nsView.layer);
    const CGFloat scale = nsWindow.backingScaleFactor > 0.0 ? nsWindow.backingScaleFactor : 1.0;
    layer.contentsScale = scale;

    CGSize drawableSize = nsView.bounds.size;
    drawableSize.width *= scale;
    drawableSize.height *= scale;
    layer.drawableSize = drawableSize;
}

- (void)handleWindowDidResize:(NSNotification*)notification
{
    if ([notification object] != glfwGetCocoaWindow(_glfwWindow))
        return;

    [self syncLayerMetrics];
    [self requestRefresh];
}

- (void)handleWindowDidChangeBackingProperties:(NSNotification*)notification
{
    if ([notification object] != glfwGetCocoaWindow(_glfwWindow))
        return;

    [self syncLayerMetrics];
    [self requestRefresh];
}

- (void)startObservingWindow:(NSWindow*)window
{
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    [center addObserver:self
               selector:@selector(handleWindowDidResize:)
                   name:NSWindowDidResizeNotification
                 object:window];
    [center addObserver:self
               selector:@selector(handleWindowDidChangeBackingProperties:)
                   name:NSWindowDidChangeBackingPropertiesNotification
                 object:window];
}

- (void)stopObservingWindow:(NSWindow*)window
{
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSWindowDidResizeNotification object:window];
    [[NSNotificationCenter defaultCenter] removeObserver:self
                                                    name:NSWindowDidChangeBackingPropertiesNotification
                                                  object:window];
}

@end

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

void InstallCocoaLiveResizeRefreshHook(GLFWwindow* window)
{
    if (window == nullptr)
        return;

    NSWindow* nsWindow = glfwGetCocoaWindow(window);
    if (nsWindow == nil)
        return;

    RTRLabCocoaWindowObserver* observer =
        (RTRLabCocoaWindowObserver*)objc_getAssociatedObject(nsWindow, &kCocoaLiveResizeObserverKey);
    if (observer != nil)
        return;

    observer = [[RTRLabCocoaWindowObserver alloc] initWithGLFWWindow:window];
    [observer startObservingWindow:nsWindow];
    objc_setAssociatedObject(nsWindow, &kCocoaLiveResizeObserverKey, observer, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    [observer release];
}

void UninstallCocoaLiveResizeRefreshHook(GLFWwindow* window)
{
    if (window == nullptr)
        return;

    NSWindow* nsWindow = glfwGetCocoaWindow(window);
    if (nsWindow == nil)
        return;

    RTRLabCocoaWindowObserver* observer =
        (RTRLabCocoaWindowObserver*)objc_getAssociatedObject(nsWindow, &kCocoaLiveResizeObserverKey);
    if (observer == nil)
        return;

    [observer stopObservingWindow:nsWindow];
    objc_setAssociatedObject(nsWindow, &kCocoaLiveResizeObserverKey, nil, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}
