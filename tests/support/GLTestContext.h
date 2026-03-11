#pragma once

struct GLFWwindow;

class GlTestContext
{
public:
    GlTestContext();
    ~GlTestContext();

    GlTestContext(const GlTestContext &) = delete;
    GlTestContext &operator=(const GlTestContext &) = delete;

    GLFWwindow *GetWindow() const { return m_Window; }
    bool IsValid() const { return m_Initialized; }

private:
    GLFWwindow *m_Window = nullptr;
    bool m_Initialized = false;
};