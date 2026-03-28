#include <array>
#include <cstddef>
#include <cstring>

#include <gtest/gtest.h>

#include <glm/glm.hpp>

#include "FakeRenderBackend.h"
#include "graphics/GraphicsDevice.h"
#include "graphics/Material.h"
#include "graphics/MeshFactory.h"
#include "renderer/RenderContext.h"
#include "renderer/SceneRenderer.h"
#include "renderer/passes/ForwardPass.h"
#include "renderer/passes/ShadowPass.h"
#include "renderer/passes/TexturePreviewPass.h"
#include "scene/Camera.h"
#include "scene/SceneData.h"

namespace
{
    constexpr ShaderBindingPoint kPreviewParamsBinding{0, 0};
    constexpr ShaderBindingPoint kPreviewTextureBinding{0, 1};
    constexpr ShaderBindingPoint kForwardParamsBinding{0, 0};
    constexpr ShaderBindingPoint kForwardShadowTextureBinding{0, 1};
    constexpr ShaderBindingPoint kShadowParamsBinding{0, 0};

    struct alignas(16) ShadowParamsCapture
    {
        glm::mat4 LightViewProjection;
        glm::mat4 Model;
    };

    struct alignas(16) ForwardParamsCapture
    {
        glm::mat4 ViewProjection;
        glm::mat4 Model;
        glm::mat4 NormalMatrix;
        glm::mat4 LightViewProjection;
        glm::vec3 CameraPosition;
        float Pad0{};
        glm::vec3 LightDirection;
        float Pad1{};
        glm::vec3 LightColor;
        float LightIntensity{};
        glm::vec3 Albedo;
        float SpecularPower{};
        float AmbientStrength{};
        int32_t UseAlbedoMap{};
        glm::vec2 ShadowMapTexelSize;
    };

    template <typename T>
    T ReadUniformBytes(const std::vector<std::byte> &bytes)
    {
        T value{};
        if (bytes.size() < sizeof(T))
            return value;

        std::memcpy(&value, bytes.data(), sizeof(T));
        return value;
    }

    Ref<FakeShader> FindShader(const Ref<FakeGraphicsDevice> &device, const char *name)
    {
        for (const auto &shader : device->CreatedShaders)
        {
            if (shader && shader->GetName() == name)
                return shader;
        }

        return nullptr;
    }

    Ref<ITexture2D> GetBoundTexture(const Ref<FakeShader> &shader, ShaderBindingPoint binding)
    {
        if (!shader)
            return nullptr;

        const auto it = shader->LogicalBoundTextures.find(binding);
        if (it == shader->LogicalBoundTextures.end())
            return nullptr;

        return it->second;
    }

    Ref<FakeUniformBuffer> GetBoundUniformBuffer(const Ref<FakeShader> &shader, ShaderBindingPoint binding)
    {
        if (!shader)
            return nullptr;

        const auto it = shader->LogicalBoundUniformBuffers.find(binding);
        if (it == shader->LogicalBoundUniformBuffers.end())
            return nullptr;

        return std::dynamic_pointer_cast<FakeUniformBuffer>(it->second);
    }

    void ExpectVec2Near(const glm::vec2 &actual, const glm::vec2 &expected, float epsilon = 1e-5f)
    {
        EXPECT_NEAR(actual.x, expected.x, epsilon);
        EXPECT_NEAR(actual.y, expected.y, epsilon);
    }

    void ExpectVec3Near(const glm::vec3 &actual, const glm::vec3 &expected, float epsilon = 1e-5f)
    {
        EXPECT_NEAR(actual.x, expected.x, epsilon);
        EXPECT_NEAR(actual.y, expected.y, epsilon);
        EXPECT_NEAR(actual.z, expected.z, epsilon);
    }

    void ExpectMat4Near(const glm::mat4 &actual, const glm::mat4 &expected, float epsilon = 1e-5f)
    {
        for (int column = 0; column < 4; ++column)
        {
            for (int row = 0; row < 4; ++row)
                EXPECT_NEAR(actual[column][row], expected[column][row], epsilon);
        }
    }

    RenderItem MakeValidRenderItem()
    {
        auto material = CreateRef<Material>();
        material->SetVec3("u_Albedo", {0.25f, 0.5f, 0.75f});
        material->SetFloat("u_SpecularPower", 16.0f);
        material->SetFloat("u_AmbientStrength", 0.2f);

        return {
            MeshFactory::CreateFullscreenQuad(),
            material,
            {}};
    }
}

class SharedRenderPassContractTests : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_Device = CreateRef<FakeGraphicsDevice>();
        SetDevice(m_Device);
    }

    void TearDown() override
    {
        SetDevice(nullptr);
        m_Device.reset();
    }

    Ref<FakeGraphicsDevice> m_Device;
};

TEST_F(SharedRenderPassContractTests, ForwardPassUsesExpectedDescriptorAndPipelineState)
{
    ForwardPass pass(64, 32, true, {0.2f, 0.4f, 0.6f, 1.0f});
    SceneData scene;
    Camera camera;
    SceneView view{scene, camera, 64, 32};
    FrameResources resources;
    resources.LightViewProjection = glm::mat4(1.0f);
    RenderContext ctx{view, SceneRendererSpecification{}, resources, SceneRendererOutput::FinalColor};

    pass.Execute(ctx);

    ASSERT_EQ(m_Device->RenderCommand->BeginRenderPasses.size(), 1u);
    ASSERT_EQ(m_Device->RenderCommand->PipelineStates.size(), 1u);
    ASSERT_EQ(m_Device->RenderCommand->Viewports.size(), 1u);

    const auto &begin = m_Device->RenderCommand->BeginRenderPasses[0];
    EXPECT_EQ(begin.Target->GetFramebuffer(), pass.GetFramebuffer());
    EXPECT_EQ(begin.Descriptor.ColorLoadAction, LoadAction::Clear);
    EXPECT_EQ(begin.Descriptor.ColorStoreAction, StoreAction::Store);
    EXPECT_EQ(begin.Descriptor.DepthLoadAction, LoadAction::Clear);
    EXPECT_EQ(begin.Descriptor.DepthStoreAction, StoreAction::Store);
    EXPECT_NEAR(begin.Descriptor.ClearColor.r, 0.2f, 1e-6f);
    EXPECT_NEAR(begin.Descriptor.ClearColor.g, 0.4f, 1e-6f);
    EXPECT_NEAR(begin.Descriptor.ClearColor.b, 0.6f, 1e-6f);
    EXPECT_NEAR(begin.Descriptor.ClearDepth, 1.0f, 1e-6f);

    const auto &pipeline = m_Device->RenderCommand->PipelineStates[0];
    EXPECT_TRUE(pipeline.DepthTestEnabled);
    EXPECT_TRUE(pipeline.DepthWriteEnabled);
    EXPECT_FALSE(pipeline.BlendEnabled);
    EXPECT_TRUE(pipeline.CullFaceEnabled);
    EXPECT_FALSE(pipeline.CullFront);

    const auto &viewport = m_Device->RenderCommand->Viewports[0];
    EXPECT_EQ(viewport.Width, 64u);
    EXPECT_EQ(viewport.Height, 32u);

    const auto shader = FindShader(m_Device, "ForwardLit");
    ASSERT_NE(shader, nullptr);
    ASSERT_EQ(shader->TextureBindEvents.size(), 1u);
    EXPECT_EQ(shader->TextureBindEvents[0].BindingPoint, kForwardShadowTextureBinding);

    auto *fallbackShadow = dynamic_cast<FakeTexture2D *>(GetBoundTexture(shader, kForwardShadowTextureBinding).get());
    ASSERT_NE(fallbackShadow, nullptr);
    EXPECT_EQ(fallbackShadow->GetWidth(), 1u);
    EXPECT_EQ(fallbackShadow->GetHeight(), 1u);
}

TEST_F(SharedRenderPassContractTests, ForwardPassSkipsInvalidItemsAndUploadsOneUniformBlockPerValidItem)
{
    ForwardPass pass(32, 16, true);

    SceneData scene;
    scene.RenderItems.push_back(MakeValidRenderItem());
    scene.RenderItems.push_back(RenderItem{});
    scene.RenderItems.push_back({MeshFactory::CreateFullscreenQuad(), nullptr, {}});
    scene.RenderItems.push_back({nullptr, CreateRef<Material>(), {}});

    Camera camera;
    SceneView view{scene, camera, 32, 16};
    FrameResources resources;
    resources.LightViewProjection = glm::mat4(1.0f);
    RenderContext ctx{view, SceneRendererSpecification{}, resources, SceneRendererOutput::FinalColor};

    pass.Execute(ctx);

    const auto shader = FindShader(m_Device, "ForwardLit");
    ASSERT_NE(shader, nullptr);
    ASSERT_EQ(shader->UniformBufferBindEvents.size(), 1u);
    EXPECT_EQ(shader->UniformBufferBindEvents[0].BindingPoint, kForwardParamsBinding);
    ASSERT_NE(GetBoundUniformBuffer(shader, kForwardParamsBinding), nullptr);
    EXPECT_EQ(m_Device->RenderCommand->DrawIndexedCalls.size(), 1u);
}

TEST_F(SharedRenderPassContractTests, ShadowPassUsesExpectedDescriptorAndPipelineStateAndSkipsInvalidItems)
{
    ShadowPass pass(128, 96);

    SceneData scene;
    scene.RenderItems.push_back(MakeValidRenderItem());
    scene.RenderItems.push_back(RenderItem{});

    Camera camera;
    SceneView view{scene, camera, 64, 48};
    FrameResources resources;
    resources.LightViewProjection = glm::mat4(1.0f);
    RenderContext ctx{view, SceneRendererSpecification{}, resources, SceneRendererOutput::FinalColor};

    pass.Execute(ctx);

    ASSERT_EQ(m_Device->RenderCommand->BeginRenderPasses.size(), 1u);
    ASSERT_EQ(m_Device->RenderCommand->PipelineStates.size(), 1u);
    ASSERT_EQ(m_Device->RenderCommand->Viewports.size(), 1u);

    const auto &begin = m_Device->RenderCommand->BeginRenderPasses[0];
    EXPECT_EQ(begin.Target->GetFramebuffer(), pass.GetFramebuffer());
    EXPECT_EQ(begin.Descriptor.ColorLoadAction, LoadAction::DontCare);
    EXPECT_EQ(begin.Descriptor.ColorStoreAction, StoreAction::DontCare);
    EXPECT_EQ(begin.Descriptor.DepthLoadAction, LoadAction::Clear);
    EXPECT_EQ(begin.Descriptor.DepthStoreAction, StoreAction::Store);
    EXPECT_NEAR(begin.Descriptor.ClearDepth, 1.0f, 1e-6f);

    const auto &pipeline = m_Device->RenderCommand->PipelineStates[0];
    EXPECT_TRUE(pipeline.DepthTestEnabled);
    EXPECT_TRUE(pipeline.DepthWriteEnabled);
    EXPECT_FALSE(pipeline.BlendEnabled);
    EXPECT_TRUE(pipeline.CullFaceEnabled);
    EXPECT_TRUE(pipeline.CullFront);

    const auto &viewport = m_Device->RenderCommand->Viewports[0];
    EXPECT_EQ(viewport.Width, 128u);
    EXPECT_EQ(viewport.Height, 96u);

    const auto shader = FindShader(m_Device, "ShadowDepth");
    ASSERT_NE(shader, nullptr);
    ASSERT_EQ(shader->UniformBufferBindEvents.size(), 1u);
    EXPECT_EQ(shader->UniformBufferBindEvents[0].BindingPoint, kShadowParamsBinding);
    ASSERT_NE(GetBoundUniformBuffer(shader, kShadowParamsBinding), nullptr);
    EXPECT_EQ(m_Device->RenderCommand->DrawIndexedCalls.size(), 1u);
}

TEST_F(SharedRenderPassContractTests, TexturePreviewPassUsesExpectedDescriptorPipelineAndModeSelection)
{
    TexturePreviewPass pass;

    TextureSpecification spec;
    spec.Width = 8;
    spec.Height = 4;
    spec.Format = TextureFormat::RGBA8;

    auto sceneColor = m_Device->CreateTexture2D(spec);
    auto shadowMap = m_Device->CreateTexture2D(spec);
    auto backBuffer = m_Device->CreateRenderTargetBackBuffer(8, 4);

    SceneData scene;
    Camera camera;
    SceneView view{scene, camera, 8, 4};
    FrameResources resources;
    resources.SceneColor = sceneColor;
    resources.ShadowMap = shadowMap;
    resources.BackBuffer = backBuffer;

    RenderContext finalCtx{view, SceneRendererSpecification{}, resources, SceneRendererOutput::FinalColor};
    pass.Execute(finalCtx);

    ASSERT_EQ(m_Device->RenderCommand->BeginRenderPasses.size(), 1u);
    ASSERT_EQ(m_Device->RenderCommand->PipelineStates.size(), 1u);
    ASSERT_EQ(m_Device->RenderCommand->DrawIndexedCalls.size(), 1u);

    const auto &finalBegin = m_Device->RenderCommand->BeginRenderPasses[0];
    EXPECT_EQ(finalBegin.Target, backBuffer);
    EXPECT_EQ(finalBegin.Descriptor.ColorLoadAction, LoadAction::Clear);
    EXPECT_EQ(finalBegin.Descriptor.ColorStoreAction, StoreAction::Store);
    EXPECT_EQ(finalBegin.Descriptor.DepthLoadAction, LoadAction::DontCare);
    EXPECT_EQ(finalBegin.Descriptor.DepthStoreAction, StoreAction::DontCare);

    const auto &pipeline = m_Device->RenderCommand->PipelineStates[0];
    EXPECT_FALSE(pipeline.DepthTestEnabled);
    EXPECT_FALSE(pipeline.DepthWriteEnabled);
    EXPECT_FALSE(pipeline.BlendEnabled);
    EXPECT_FALSE(pipeline.CullFaceEnabled);

    auto previewShader = FindShader(m_Device, "TexturePreview");
    ASSERT_NE(previewShader, nullptr);
    ASSERT_EQ(previewShader->TextureBindEvents.size(), 1u);
    EXPECT_EQ(previewShader->TextureBindEvents[0].BindingPoint, kPreviewTextureBinding);
    EXPECT_EQ(GetBoundTexture(previewShader, kPreviewTextureBinding), sceneColor);
    auto previewBuffer = GetBoundUniformBuffer(previewShader, kPreviewParamsBinding);
    ASSERT_NE(previewBuffer, nullptr);
    EXPECT_EQ(previewBuffer->LastSetSize, sizeof(int32_t));
    EXPECT_EQ(ReadUniformBytes<int32_t>(previewBuffer->Bytes), 0);

    m_Device->RenderCommand->Reset();

    RenderContext shadowCtx{view, SceneRendererSpecification{}, resources, SceneRendererOutput::ShadowMap};
    pass.Execute(shadowCtx);

    ASSERT_EQ(previewShader->TextureBindEvents.size(), 2u);
    EXPECT_EQ(previewShader->TextureBindEvents.back().BindingPoint, kPreviewTextureBinding);
    EXPECT_EQ(GetBoundTexture(previewShader, kPreviewTextureBinding), shadowMap);
    previewBuffer = GetBoundUniformBuffer(previewShader, kPreviewParamsBinding);
    ASSERT_NE(previewBuffer, nullptr);
    EXPECT_EQ(ReadUniformBytes<int32_t>(previewBuffer->Bytes), 1);
}

TEST_F(SharedRenderPassContractTests, SceneRendererPreparesConsistentFrameResourcesForPasses)
{
    SceneRendererSpecification spec;
    spec.ShadowMapWidth = 256;
    spec.ShadowMapHeight = 128;

    SceneRenderer renderer(80, 40, spec);
    renderer.SetOutputMode(SceneRendererOutput::FinalColor);

    SceneData scene;
    scene.MainDirectionalLight.Direction = {-1.0f, -2.0f, -3.0f};
    scene.MainDirectionalLight.Color = {0.2f, 0.5f, 0.8f};
    scene.MainDirectionalLight.Intensity = 3.5f;
    scene.RenderItems.push_back(MakeValidRenderItem());

    Camera camera;

    renderer.Render(scene, camera);

    const auto shadowShader = FindShader(m_Device, "ShadowDepth");
    const auto forwardShader = FindShader(m_Device, "ForwardLit");
    const auto previewShader = FindShader(m_Device, "TexturePreview");

    ASSERT_NE(shadowShader, nullptr);
    ASSERT_NE(forwardShader, nullptr);
    ASSERT_NE(previewShader, nullptr);
    const auto shadowBuffer = GetBoundUniformBuffer(shadowShader, kShadowParamsBinding);
    const auto forwardBuffer = GetBoundUniformBuffer(forwardShader, kForwardParamsBinding);
    const auto previewBuffer = GetBoundUniformBuffer(previewShader, kPreviewParamsBinding);
    ASSERT_NE(shadowBuffer, nullptr);
    ASSERT_NE(forwardBuffer, nullptr);
    ASSERT_NE(previewBuffer, nullptr);
    ASSERT_EQ(shadowBuffer->LastSetSize, sizeof(ShadowParamsCapture));
    ASSERT_EQ(forwardBuffer->LastSetSize, sizeof(ForwardParamsCapture));

    const auto shadowParams = ReadUniformBytes<ShadowParamsCapture>(shadowBuffer->Bytes);
    const auto forwardParams = ReadUniformBytes<ForwardParamsCapture>(forwardBuffer->Bytes);

    ExpectMat4Near(forwardParams.LightViewProjection, shadowParams.LightViewProjection);
    ExpectVec3Near(forwardParams.CameraPosition, camera.GetPosition());
    ExpectVec3Near(forwardParams.LightDirection, scene.MainDirectionalLight.Direction);
    ExpectVec3Near(forwardParams.LightColor, scene.MainDirectionalLight.Color);
    EXPECT_NEAR(forwardParams.LightIntensity, scene.MainDirectionalLight.Intensity, 1e-6f);
    ExpectVec2Near(
        forwardParams.ShadowMapTexelSize,
        {1.0f / static_cast<float>(spec.ShadowMapWidth),
         1.0f / static_cast<float>(spec.ShadowMapHeight)});

    EXPECT_EQ(GetBoundTexture(forwardShader, kForwardShadowTextureBinding), renderer.GetShadowPass()->GetDepthTexture());
    EXPECT_EQ(GetBoundTexture(previewShader, kPreviewTextureBinding),
              renderer.GetForwardPass()->GetFramebuffer()->GetColorAttachment(0));
    EXPECT_EQ(ReadUniformBytes<int32_t>(previewBuffer->Bytes), 0);
}
