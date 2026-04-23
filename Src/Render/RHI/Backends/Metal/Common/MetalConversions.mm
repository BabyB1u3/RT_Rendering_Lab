#include "Render/RHI/Backends/Metal/Common/MetalConversions.h"

#include <algorithm>
#include <cctype>

#include "Core/Diagnostics/Assert/Assert.h"
#include "Render/RHI/Backends/Common/RHIShellCommon.h"

namespace MetalRHI
{
MTLStorageMode ToMetalStorageMode(MemoryUsage memoryUsage)
{
    switch (memoryUsage)
    {
        case MemoryUsage::GpuOnly:
            return MTLStorageModePrivate;
        case MemoryUsage::CpuToGpu:
        case MemoryUsage::GpuToCpu:
            return MTLStorageModeShared;
    }

    return MTLStorageModePrivate;
}

MTLResourceOptions ToMetalBufferResourceOptions(MemoryUsage memoryUsage)
{
    return MTLResourceCPUCacheModeDefaultCache |
           static_cast<MTLResourceOptions>(ToMetalStorageMode(memoryUsage) << MTLResourceStorageModeShift);
}

MTLPixelFormat ToMetalPixelFormat(Format format)
{
    switch (format)
    {
        case Format::R8_UNORM:
            return MTLPixelFormatR8Unorm;
        case Format::RG8_UNORM:
            return MTLPixelFormatRG8Unorm;
        case Format::BGRA8_UNORM:
            return MTLPixelFormatBGRA8Unorm;
        case Format::BGRA8_SRGB:
            return MTLPixelFormatBGRA8Unorm_sRGB;
        case Format::RGBA8_UNORM:
            return MTLPixelFormatRGBA8Unorm;
        case Format::RGBA8_SRGB:
            return MTLPixelFormatRGBA8Unorm_sRGB;
        case Format::R16F:
            return MTLPixelFormatR16Float;
        case Format::RG16F:
            return MTLPixelFormatRG16Float;
        case Format::RGBA16F:
            return MTLPixelFormatRGBA16Float;
        case Format::R32F:
            return MTLPixelFormatR32Float;
        case Format::RG32F:
            return MTLPixelFormatRG32Float;
        case Format::RGBA32F:
            return MTLPixelFormatRGBA32Float;
        case Format::R32_UINT:
            return MTLPixelFormatR32Uint;
        case Format::D16_UNORM:
            return MTLPixelFormatDepth16Unorm;
        case Format::D32_SFLOAT:
            return MTLPixelFormatDepth32Float;
        case Format::D24_UNORM_S8_UINT:
            return MTLPixelFormatDepth24Unorm_Stencil8;
        case Format::D32_SFLOAT_S8_UINT:
            return MTLPixelFormatDepth32Float_Stencil8;
        default:
            RTRLAB_ASSERTF(false, "Unsupported Metal RHI format {}", static_cast<uint32_t>(format));
            return MTLPixelFormatInvalid;
    }
}

MTLTextureType ToMetalTextureType(TextureType type)
{
    switch (type)
    {
        case TextureType::Tex2D:
            return MTLTextureType2D;
        case TextureType::Tex2DArray:
            return MTLTextureType2DArray;
        case TextureType::Tex3D:
            return MTLTextureType3D;
        case TextureType::Cube:
            return MTLTextureTypeCube;
    }

    return MTLTextureType2D;
}

MTLSamplerMinMagFilter ToMetalFilter(FilterMode mode)
{
    return mode == FilterMode::Nearest ? MTLSamplerMinMagFilterNearest : MTLSamplerMinMagFilterLinear;
}

MTLSamplerMipFilter ToMetalMipFilter(MipFilterMode mode)
{
    switch (mode)
    {
        case MipFilterMode::None:
            return MTLSamplerMipFilterNotMipmapped;
        case MipFilterMode::Nearest:
            return MTLSamplerMipFilterNearest;
        case MipFilterMode::Linear:
            return MTLSamplerMipFilterLinear;
    }

    return MTLSamplerMipFilterLinear;
}

MTLSamplerAddressMode ToMetalAddressMode(AddressMode mode)
{
    switch (mode)
    {
        case AddressMode::Repeat:
            return MTLSamplerAddressModeRepeat;
        case AddressMode::MirroredRepeat:
            return MTLSamplerAddressModeMirrorRepeat;
        case AddressMode::ClampToEdge:
            return MTLSamplerAddressModeClampToEdge;
        case AddressMode::ClampToBorder:
            return MTLSamplerAddressModeClampToBorderColor;
    }

    return MTLSamplerAddressModeRepeat;
}

MTLLoadAction ToMetalLoadAction(LoadOp loadOp)
{
    switch (loadOp)
    {
        case LoadOp::Clear:
            return MTLLoadActionClear;
        case LoadOp::DontCare:
            return MTLLoadActionDontCare;
        case LoadOp::Load:
        default:
            return MTLLoadActionLoad;
    }
}

MTLStoreAction ToMetalStoreAction(StoreOp storeOp)
{
    switch (storeOp)
    {
        case StoreOp::DontCare:
            return MTLStoreActionDontCare;
        case StoreOp::Store:
        default:
            return MTLStoreActionStore;
    }
}

MTLCompareFunction ToMetalCompareFunction(CompareOp compareOp)
{
    switch (compareOp)
    {
        case CompareOp::Never:
            return MTLCompareFunctionNever;
        case CompareOp::Less:
            return MTLCompareFunctionLess;
        case CompareOp::Equal:
            return MTLCompareFunctionEqual;
        case CompareOp::LessEqual:
            return MTLCompareFunctionLessEqual;
        case CompareOp::Greater:
            return MTLCompareFunctionGreater;
        case CompareOp::NotEqual:
            return MTLCompareFunctionNotEqual;
        case CompareOp::GreaterEqual:
            return MTLCompareFunctionGreaterEqual;
        case CompareOp::Always:
        default:
            return MTLCompareFunctionAlways;
    }
}

bool IsDepthFormat(Format format)
{
    switch (format)
    {
        case Format::D16_UNORM:
        case Format::D32_SFLOAT:
        case Format::D24_UNORM_S8_UINT:
        case Format::D32_SFLOAT_S8_UINT:
            return true;
        default:
            return false;
    }
}

bool HasStencilComponent(Format format)
{
    return format == Format::D24_UNORM_S8_UINT || format == Format::D32_SFLOAT_S8_UINT;
}

uint32_t GetFormatBytesPerPixel(Format format)
{
    switch (format)
    {
        case Format::R8_UNORM:
            return 1;
        case Format::RG8_UNORM:
            return 2;
        case Format::RGBA8_UNORM:
        case Format::RGBA8_SRGB:
        case Format::BGRA8_UNORM:
        case Format::BGRA8_SRGB:
        case Format::R32F:
        case Format::R32_UINT:
        case Format::D32_SFLOAT:
            return 4;
        case Format::R16F:
        case Format::D16_UNORM:
            return 2;
        case Format::RG16F:
        case Format::RG32F:
            return 8;
        case Format::RGBA16F:
        case Format::RGBA32F:
            return 16;
        case Format::D24_UNORM_S8_UINT:
        case Format::D32_SFLOAT_S8_UINT:
            return 4;
        default:
            break;
    }

    RTRLAB_ASSERTF(false, "Unsupported Metal copy format {}", static_cast<uint32_t>(format));
    return 0;
}

bool HasDebugName(const char* debugName)
{
    return debugName != nullptr && debugName[0] != '\0';
}

NSString* MakeNSString(const char* debugName)
{
    if (!HasDebugName(debugName))
        return nil;

    return [NSString stringWithUTF8String:debugName];
}

NSString* MakeNSString(const std::vector<uint8_t>& utf8Bytes)
{
    if (utf8Bytes.empty())
        return nil;

    return [[[NSString alloc] initWithBytes:utf8Bytes.data() length:utf8Bytes.size()
                                   encoding:NSUTF8StringEncoding] autorelease];
}

void SetMetalDebugLabel(id<MTLResource> resource, const char* debugName)
{
    NSString* label = MakeNSString(debugName);
    if (resource != nil && label != nil)
        [resource setLabel:label];
}

std::string MakeTextureViewDebugName(const Texture& texture)
{
    const char* debugName = texture.GetDesc().m_DebugName;
    if (!HasDebugName(debugName))
        return {};

    return std::string(debugName) + ".View";
}

MTLVertexFormat ToMetalVertexFormat(Format format)
{
    switch (format)
    {
        case Format::RG32F:
            return MTLVertexFormatFloat2;
        case Format::RGBA32F:
            return MTLVertexFormatFloat4;
        default:
            RTRLAB_ASSERTF(false, "Unsupported Metal vertex attribute format {}", static_cast<uint32_t>(format));
            return MTLVertexFormatInvalid;
    }
}

MTLPrimitiveType ToMetalPrimitiveType(PrimitiveTopology topology)
{
    switch (topology)
    {
        case PrimitiveTopology::TriangleList:
            return MTLPrimitiveTypeTriangle;
        case PrimitiveTopology::TriangleStrip:
            return MTLPrimitiveTypeTriangleStrip;
        case PrimitiveTopology::LineList:
            return MTLPrimitiveTypeLine;
        case PrimitiveTopology::LineStrip:
            return MTLPrimitiveTypeLineStrip;
        case PrimitiveTopology::PointList:
            return MTLPrimitiveTypePoint;
    }

    return MTLPrimitiveTypeTriangle;
}

MTLIndexType ToMetalIndexType(IndexType indexType)
{
    switch (indexType)
    {
        case IndexType::UInt16:
            return MTLIndexTypeUInt16;
        case IndexType::UInt32:
            return MTLIndexTypeUInt32;
    }

    return MTLIndexTypeUInt32;
}

MTLWinding ToMetalWinding(FrontFace frontFace)
{
    return frontFace == FrontFace::CW ? MTLWindingClockwise : MTLWindingCounterClockwise;
}

MTLCullMode ToMetalCullMode(CullMode cullMode)
{
    switch (cullMode)
    {
        case CullMode::None:
            return MTLCullModeNone;
        case CullMode::Front:
            return MTLCullModeFront;
        case CullMode::Back:
            return MTLCullModeBack;
    }

    return MTLCullModeBack;
}

MTLTriangleFillMode ToMetalTriangleFillMode(FillMode fillMode)
{
    return fillMode == FillMode::Wireframe ? MTLTriangleFillModeLines : MTLTriangleFillModeFill;
}

namespace
{
bool StageMaskContains(ShaderStage mask, ShaderStage stage)
{
    return static_cast<uint32_t>(mask & stage) != 0;
}

bool IsMetalIdentifierChar(const char ch)
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_';
}

std::string_view TrimAsciiWhitespace(const std::string_view text)
{
    size_t begin = 0;
    size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin])) != 0)
        ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0)
        --end;
    return text.substr(begin, end - begin);
}

size_t FindMatchingDelimiter(const std::string_view text, size_t openIndex, const char openChar, const char closeChar)
{
    RTRLAB_ASSERT_MSG(openIndex < text.size() && text[openIndex] == openChar,
                      "FindMatchingDelimiter requires a valid opening delimiter.");
    uint32_t depth = 0;
    for (size_t index = openIndex; index < text.size(); ++index)
    {
        if (text[index] == openChar)
        {
            ++depth;
        }
        else if (text[index] == closeChar)
        {
            RTRLAB_ASSERT_MSG(depth > 0, "FindMatchingDelimiter encountered an unmatched closing delimiter.");
            --depth;
            if (depth == 0)
                return index;
        }
    }

    RTRLAB_ASSERT_MSG(false, "FindMatchingDelimiter failed to locate a matching closing delimiter.");
    return std::string_view::npos;
}

std::vector<std::string_view> SplitTopLevelCommaSeparated(const std::string_view text)
{
    std::vector<std::string_view> parts;
    size_t elementStart = 0;
    int parenDepth = 0;
    int angleDepth = 0;
    int squareDepth = 0;
    for (size_t index = 0; index < text.size(); ++index)
    {
        const char ch = text[index];
        switch (ch)
        {
            case '(':
                ++parenDepth;
                break;
            case ')':
                --parenDepth;
                break;
            case '<':
                ++angleDepth;
                break;
            case '>':
                --angleDepth;
                break;
            case '[':
                ++squareDepth;
                break;
            case ']':
                --squareDepth;
                break;
            case ',':
                if (parenDepth == 0 && angleDepth == 0 && squareDepth == 0)
                {
                    parts.push_back(TrimAsciiWhitespace(text.substr(elementStart, index - elementStart)));
                    elementStart = index + 1;
                }
                break;
            default:
                break;
        }
    }

    const std::string_view tail = TrimAsciiWhitespace(text.substr(elementStart));
    if (!tail.empty())
        parts.push_back(tail);
    return parts;
}

bool IsResourceAttributeName(std::string_view attributeName)
{
    return attributeName == "buffer" || attributeName == "texture" || attributeName == "sampler";
}

std::string ExtractTrailingIdentifier(const std::string_view declaration)
{
    size_t end = declaration.size();
    while (end > 0 && std::isspace(static_cast<unsigned char>(declaration[end - 1])) != 0)
        --end;

    size_t begin = end;
    while (begin > 0 && IsMetalIdentifierChar(declaration[begin - 1]))
        --begin;

    return begin < end ? std::string(declaration.substr(begin, end - begin)) : std::string{};
}

bool MatchesReflectedBindingName(std::string_view metalVariableName, std::string_view reflectedName)
{
    if (metalVariableName == reflectedName)
        return true;
    return metalVariableName.size() > reflectedName.size() &&
           metalVariableName.substr(0, reflectedName.size()) == reflectedName &&
           metalVariableName[reflectedName.size()] == '_';
}

struct ParsedMetalParameter
{
    std::string m_DeclarationText;
    std::string m_VariableName;
    std::string m_AttributeName;
};

bool ParseMetalResourceParameter(std::string_view parameterText, ParsedMetalParameter* outParameter)
{
    if (outParameter == nullptr)
        return false;

    parameterText = TrimAsciiWhitespace(parameterText);
    const size_t attributeBegin = parameterText.rfind("[[");
    const size_t attributeEnd = parameterText.rfind("]]");
    if (attributeBegin == std::string_view::npos || attributeEnd == std::string_view::npos ||
        attributeEnd < attributeBegin)
    {
        return false;
    }

    const std::string_view declarationText = TrimAsciiWhitespace(parameterText.substr(0, attributeBegin));
    const std::string_view attributeText =
        TrimAsciiWhitespace(parameterText.substr(attributeBegin + 2, attributeEnd - (attributeBegin + 2)));
    const size_t parenBegin = attributeText.find('(');
    if (parenBegin == std::string_view::npos)
        return false;

    const std::string_view attributeName = TrimAsciiWhitespace(attributeText.substr(0, parenBegin));
    if (!IsResourceAttributeName(attributeName))
        return false;

    ParsedMetalParameter parameter;
    parameter.m_DeclarationText = std::string(declarationText);
    parameter.m_VariableName = ExtractTrailingIdentifier(declarationText);
    parameter.m_AttributeName = std::string(attributeName);
    if (parameter.m_DeclarationText.empty() || parameter.m_VariableName.empty())
        return false;

    *outParameter = std::move(parameter);
    return true;
}

void ReplaceIdentifierInText(std::string& text, std::string_view identifier, std::string_view replacement)
{
    if (identifier.empty())
        return;

    size_t searchOffset = 0;
    while ((searchOffset = text.find(identifier, searchOffset)) != std::string::npos)
    {
        const size_t nameBegin = searchOffset;
        const size_t nameEnd = nameBegin + identifier.size();
        const bool validLeadingBoundary = nameBegin == 0 || !IsMetalIdentifierChar(text[nameBegin - 1]);
        const bool validTrailingBoundary = nameEnd >= text.size() || !IsMetalIdentifierChar(text[nameEnd]);
        if (!validLeadingBoundary || !validTrailingBoundary)
        {
            searchOffset = nameEnd;
            continue;
        }

        text.replace(nameBegin, identifier.size(), replacement);
        searchOffset = nameBegin + replacement.size();
    }
}

std::string GetMetalArgumentBufferStructName(uint32_t setIndex, ShaderStage stage)
{
    switch (stage)
    {
        case ShaderStage::Vertex:
            return "__RTRMetalSet" + std::to_string(setIndex) + "_VertexArgs";
        case ShaderStage::Fragment:
            return "__RTRMetalSet" + std::to_string(setIndex) + "_FragmentArgs";
        case ShaderStage::Compute:
            return "__RTRMetalSet" + std::to_string(setIndex) + "_ComputeArgs";
        default:
            break;
    }

    return "__RTRMetalSet" + std::to_string(setIndex) + "_Args";
}

std::string GetMetalArgumentBufferParameterName(uint32_t setIndex)
{
    return "__rtr_set_" + std::to_string(setIndex);
}

const MetalStageBindingPlan* FindStageBindingPlan(const MetalSetBindingPlan& setPlan, ShaderStage stage)
{
    const auto it =
        std::find_if(setPlan.m_StagePlans.begin(),
                     setPlan.m_StagePlans.end(),
                     [stage](const MetalStageBindingPlan& stagePlan) { return stagePlan.m_Stage == stage; });
    return it != setPlan.m_StagePlans.end() ? &(*it) : nullptr;
}

const MetalBindingPlanEntry* FindBindingPlanEntryForVariable(const MetalStageBindingPlan& stagePlan,
                                                             std::string_view variableName)
{
    const auto it = std::find_if(stagePlan.m_Entries.begin(),
                                 stagePlan.m_Entries.end(),
                                 [variableName](const MetalBindingPlanEntry& entry)
                                 { return MatchesReflectedBindingName(variableName, entry.m_Name); });
    return it != stagePlan.m_Entries.end() ? &(*it) : nullptr;
}
} // namespace

std::string RewriteMetalShaderSourceForArgumentBuffers(const CompiledShaderProgramDesc& desc,
                                                       const CompiledShaderBlob& blob,
                                                       std::string_view sourceText,
                                                       std::vector<MetalSetBindingPlan>* outUsedSetPlans)
{
    if (blob.m_Backend != BackendType::Metal || blob.m_Stage == ShaderStage::None || sourceText.empty() ||
        blob.m_EntryPoint.empty())
    {
        return std::string(sourceText);
    }

    const PipelineLayoutDesc pipelineLayoutDesc = RHIInternal::BuildPipelineLayoutDescFromReflection(desc.m_Reflection);
    const std::vector<MetalSetBindingPlan> bindingPlans = BuildMetalSetBindingPlans(pipelineLayoutDesc);

    const std::string entryPointSignature = blob.m_EntryPoint + "(";
    const size_t entryPointNamePos = sourceText.find(entryPointSignature);
    RTRLAB_ASSERT_MSG(entryPointNamePos != std::string_view::npos,
                      "Metal shader source rewrite failed to locate the entry-point name.");
    const size_t functionStart = sourceText.rfind('\n', entryPointNamePos) == std::string_view::npos
                                     ? 0
                                     : sourceText.rfind('\n', entryPointNamePos) + 1;
    const size_t openParen = sourceText.find('(', entryPointNamePos);
    RTRLAB_ASSERT_MSG(openParen != std::string_view::npos,
                      "Metal shader source rewrite failed to locate the entry-point parameter list.");
    const size_t closeParen = FindMatchingDelimiter(sourceText, openParen, '(', ')');
    const size_t openBrace = sourceText.find('{', closeParen);
    RTRLAB_ASSERT_MSG(openBrace != std::string_view::npos,
                      "Metal shader source rewrite failed to locate the entry-point body.");
    const size_t closeBrace = FindMatchingDelimiter(sourceText, openBrace, '{', '}');

    const std::string_view parameterListText = sourceText.substr(openParen + 1, closeParen - openParen - 1);
    const std::vector<std::string_view> originalParameters = SplitTopLevelCommaSeparated(parameterListText);

    struct StageSetRewrite
    {
        uint32_t m_SetIndex = 0;
        uint32_t m_ArgumentBufferSlot = 0;
        std::vector<std::string> m_MemberDeclarations;
        std::vector<std::pair<std::string, std::string>> m_BodyReplacements;
        std::vector<MetalBindingPlanEntry> m_Entries;
    };

    std::vector<StageSetRewrite> setRewrites;
    std::vector<std::string> rewrittenParameters;

    auto findOrAddSetRewrite = [&setRewrites](uint32_t setIndex, uint32_t slot) -> StageSetRewrite&
    {
        const auto it =
            std::find_if(setRewrites.begin(),
                         setRewrites.end(),
                         [setIndex](const StageSetRewrite& rewrite) { return rewrite.m_SetIndex == setIndex; });
        if (it != setRewrites.end())
            return *it;

        setRewrites.push_back({});
        setRewrites.back().m_SetIndex = setIndex;
        setRewrites.back().m_ArgumentBufferSlot = slot;
        return setRewrites.back();
    };

    for (std::string_view parameterText : originalParameters)
    {
        ParsedMetalParameter parsedParameter;
        if (!ParseMetalResourceParameter(parameterText, &parsedParameter))
        {
            rewrittenParameters.push_back(std::string(TrimAsciiWhitespace(parameterText)));
            continue;
        }

        bool matchedSet = false;
        for (const MetalSetBindingPlan& setPlan : bindingPlans)
        {
            const MetalStageBindingPlan* stagePlan = FindStageBindingPlan(setPlan, blob.m_Stage);
            if (stagePlan == nullptr)
                continue;

            const MetalBindingPlanEntry* entry =
                FindBindingPlanEntryForVariable(*stagePlan, parsedParameter.m_VariableName);
            if (entry == nullptr)
                continue;

            StageSetRewrite& setRewrite = findOrAddSetRewrite(setPlan.m_SetIndex, stagePlan->m_ArgumentBufferSlot);
            const std::string memberDeclaration =
                parsedParameter.m_DeclarationText + " [[id(" + std::to_string(entry->m_ArgumentIndex) + ")]];";
            const bool alreadyAddedMember = std::find(setRewrite.m_MemberDeclarations.begin(),
                                                      setRewrite.m_MemberDeclarations.end(),
                                                      memberDeclaration) != setRewrite.m_MemberDeclarations.end();
            if (!alreadyAddedMember)
            {
                if (setRewrite.m_MemberDeclarations.empty())
                {
                    const std::string structName = GetMetalArgumentBufferStructName(setPlan.m_SetIndex, blob.m_Stage);
                    const std::string parameterName = GetMetalArgumentBufferParameterName(setPlan.m_SetIndex);
                    rewrittenParameters.push_back("constant " + structName + "& " + parameterName + " [[buffer(" +
                                                  std::to_string(stagePlan->m_ArgumentBufferSlot) + ")]]");
                }
                setRewrite.m_MemberDeclarations.push_back(memberDeclaration);
                setRewrite.m_Entries.push_back(*entry);
            }

            setRewrite.m_BodyReplacements.push_back(
                {parsedParameter.m_VariableName,
                 GetMetalArgumentBufferParameterName(setPlan.m_SetIndex) + "." + parsedParameter.m_VariableName});
            matchedSet = true;
            break;
        }

        RTRLAB_ASSERT_MSG(matchedSet, "Metal shader source rewrite failed to map a resource parameter to a set plan.");
    }

    if (setRewrites.empty())
        return std::string(sourceText);

    std::sort(setRewrites.begin(),
              setRewrites.end(),
              [](const StageSetRewrite& lhs, const StageSetRewrite& rhs) { return lhs.m_SetIndex < rhs.m_SetIndex; });

    if (outUsedSetPlans != nullptr)
    {
        outUsedSetPlans->clear();
        outUsedSetPlans->reserve(setRewrites.size());
        for (const StageSetRewrite& setRewrite : setRewrites)
        {
            MetalSetBindingPlan usedPlan;
            usedPlan.m_SetIndex = setRewrite.m_SetIndex;
            MetalStageBindingPlan usedStagePlan;
            usedStagePlan.m_Stage = blob.m_Stage;
            usedStagePlan.m_ArgumentBufferSlot = setRewrite.m_ArgumentBufferSlot;
            usedStagePlan.m_Entries = setRewrite.m_Entries;
            usedPlan.m_StagePlans.push_back(std::move(usedStagePlan));
            outUsedSetPlans->push_back(std::move(usedPlan));
        }
    }

    std::string generatedStructs;
    for (const StageSetRewrite& setRewrite : setRewrites)
    {
        generatedStructs += "struct " + GetMetalArgumentBufferStructName(setRewrite.m_SetIndex, blob.m_Stage) + "\n{\n";
        for (const std::string& member : setRewrite.m_MemberDeclarations)
            generatedStructs += "    " + member + "\n";
        generatedStructs += "};\n\n";
    }

    std::string rewrittenBody(sourceText.substr(openBrace, closeBrace - openBrace + 1));
    for (const StageSetRewrite& setRewrite : setRewrites)
    {
        for (const auto& replacement : setRewrite.m_BodyReplacements)
            ReplaceIdentifierInText(rewrittenBody, replacement.first, replacement.second);
    }

    std::string rewrittenHeader(sourceText.substr(functionStart, openParen - functionStart));
    rewrittenHeader.push_back('(');
    for (size_t index = 0; index < rewrittenParameters.size(); ++index)
    {
        if (index > 0)
            rewrittenHeader += ", ";
        rewrittenHeader += rewrittenParameters[index];
    }
    rewrittenHeader.push_back(')');

    std::string rewrittenSource;
    rewrittenSource.reserve(sourceText.size() + generatedStructs.size() + 128);
    rewrittenSource.append(sourceText.substr(0, functionStart));
    rewrittenSource.append(generatedStructs);
    rewrittenSource.append(rewrittenHeader);
    rewrittenSource.append(rewrittenBody);
    rewrittenSource.append(sourceText.substr(closeBrace + 1));
    return rewrittenSource;
}

void MergeUsedMetalSetBindingPlans(std::vector<MetalSetBindingPlan>* destination,
                                   std::vector<MetalSetBindingPlan> source)
{
    if (destination == nullptr)
        return;

    for (MetalSetBindingPlan& candidate : source)
    {
        const auto existingSetIt = std::find_if(destination->begin(),
                                                destination->end(),
                                                [setIndex = candidate.m_SetIndex](const MetalSetBindingPlan& existing)
                                                { return existing.m_SetIndex == setIndex; });
        if (existingSetIt == destination->end())
        {
            destination->push_back(std::move(candidate));
            continue;
        }

        for (MetalStageBindingPlan& candidateStage : candidate.m_StagePlans)
        {
            const auto existingStageIt =
                std::find_if(existingSetIt->m_StagePlans.begin(),
                             existingSetIt->m_StagePlans.end(),
                             [stage = candidateStage.m_Stage](const MetalStageBindingPlan& existingStage)
                             { return existingStage.m_Stage == stage; });
            if (existingStageIt == existingSetIt->m_StagePlans.end())
            {
                existingSetIt->m_StagePlans.push_back(std::move(candidateStage));
                continue;
            }

            existingStageIt->m_ArgumentBufferSlot = candidateStage.m_ArgumentBufferSlot;
            existingStageIt->m_Entries = std::move(candidateStage.m_Entries);
        }
    }

    std::sort(destination->begin(),
              destination->end(),
              [](const MetalSetBindingPlan& lhs, const MetalSetBindingPlan& rhs)
              { return lhs.m_SetIndex < rhs.m_SetIndex; });
}

std::vector<MetalSetBindingPlan> BuildMetalSetBindingPlans(const PipelineLayoutDesc& desc)
{
    std::vector<MetalSetBindingPlan> plans;

    auto findOrAddPlan = [&plans](uint32_t setIndex) -> MetalSetBindingPlan*
    {
        const auto it =
            std::find_if(plans.begin(),
                         plans.end(),
                         [setIndex](const MetalSetBindingPlan& plan) { return plan.m_SetIndex == setIndex; });
        if (it != plans.end())
            return &(*it);

        plans.push_back(MetalSetBindingPlan{});
        plans.back().m_SetIndex = setIndex;
        return &plans.back();
    };

    auto findOrAddStagePlan =
        [](MetalSetBindingPlan& setPlan, ShaderStage stage, uint32_t slot) -> MetalStageBindingPlan&
    {
        const auto it =
            std::find_if(setPlan.m_StagePlans.begin(),
                         setPlan.m_StagePlans.end(),
                         [stage](const MetalStageBindingPlan& stagePlan) { return stagePlan.m_Stage == stage; });
        if (it != setPlan.m_StagePlans.end())
            return *it;

        setPlan.m_StagePlans.push_back({});
        setPlan.m_StagePlans.back().m_Stage = stage;
        setPlan.m_StagePlans.back().m_ArgumentBufferSlot = slot;
        return setPlan.m_StagePlans.back();
    };

    uint32_t vertexSlotCounter = 0;
    uint32_t fragmentSlotCounter = 0;
    uint32_t computeSlotCounter = 0;
    for (const BindingInfo& binding : desc.m_Bindings)
    {
        MetalSetBindingPlan* setPlan = findOrAddPlan(binding.m_SetIndex);

        MetalBindingPlanEntry entry;
        entry.m_Name = binding.m_Name;
        entry.m_Binding = binding.m_Binding;
        entry.m_ArrayCount = binding.m_ArrayCount;
        entry.m_Kind = binding.m_Kind;

        if (StageMaskContains(binding.m_StageMask, ShaderStage::Vertex))
            findOrAddStagePlan(*setPlan, ShaderStage::Vertex, vertexSlotCounter).m_Entries.push_back(entry);
        if (StageMaskContains(binding.m_StageMask, ShaderStage::Fragment))
            findOrAddStagePlan(*setPlan, ShaderStage::Fragment, fragmentSlotCounter).m_Entries.push_back(entry);
        if (StageMaskContains(binding.m_StageMask, ShaderStage::Compute))
            findOrAddStagePlan(*setPlan, ShaderStage::Compute, computeSlotCounter).m_Entries.push_back(entry);
    }

    for (MetalSetBindingPlan& setPlan : plans)
    {
        std::sort(setPlan.m_StagePlans.begin(),
                  setPlan.m_StagePlans.end(),
                  [](const MetalStageBindingPlan& lhs, const MetalStageBindingPlan& rhs)
                  { return static_cast<uint32_t>(lhs.m_Stage) < static_cast<uint32_t>(rhs.m_Stage); });
    }

    uint32_t assignedVertexSets = 0;
    uint32_t assignedFragmentSets = 0;
    uint32_t assignedComputeSets = 0;
    for (MetalSetBindingPlan& setPlan : plans)
    {
        for (MetalStageBindingPlan& stagePlan : setPlan.m_StagePlans)
        {
            switch (stagePlan.m_Stage)
            {
                case ShaderStage::Vertex:
                    stagePlan.m_ArgumentBufferSlot = assignedVertexSets++;
                    break;
                case ShaderStage::Fragment:
                    stagePlan.m_ArgumentBufferSlot = assignedFragmentSets++;
                    break;
                case ShaderStage::Compute:
                    stagePlan.m_ArgumentBufferSlot = assignedComputeSets++;
                    break;
                default:
                    break;
            }
        }
    }

    for (MetalSetBindingPlan& setPlan : plans)
    {
        for (MetalStageBindingPlan& stagePlan : setPlan.m_StagePlans)
        {
            uint32_t nextArgumentIndex = 0;
            for (MetalBindingPlanEntry& entry : stagePlan.m_Entries)
            {
                entry.m_ArgumentIndex = nextArgumentIndex;
                nextArgumentIndex += std::max(entry.m_ArrayCount, 1u);
            }
        }
    }

    return plans;
}

uint32_t ComputeVertexBufferSlotBase(const std::vector<MetalSetBindingPlan>& plans)
{
    uint32_t maxVertexResourceSlot = 0;
    bool sawVertexResource = false;

    for (const MetalSetBindingPlan& plan : plans)
    {
        const MetalStageBindingPlan* stagePlan = FindStageBindingPlan(plan, ShaderStage::Vertex);
        if (stagePlan == nullptr)
            continue;

        maxVertexResourceSlot = std::max(maxVertexResourceSlot, stagePlan->m_ArgumentBufferSlot);
        sawVertexResource = true;
    }

    return sawVertexResource ? maxVertexResourceSlot + 1u : 0u;
}
} // namespace MetalRHI
