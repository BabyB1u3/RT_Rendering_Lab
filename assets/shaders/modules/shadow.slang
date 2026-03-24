// shadow.slang
//
// Shared shadow-sampling utilities.
// Usage: import shadow;  then call ComputeShadow().

module shadow;

/// 3x3 PCF shadow sampling.
/// Returns 0.0 (fully lit) to 1.0 (fully shadowed).
public float ComputeShadow(
    Sampler2D    shadowMap,
    float4       lightSpacePosition,
    float3       normal,
    float3       lightDir)
{
    float3 projCoords = lightSpacePosition.xyz / lightSpacePosition.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0)
        return 0.0;

    float currentDepth = projCoords.z;
    float bias = max(0.0001, 0.002 * (1.0 - dot(normalize(normal), normalize(-lightDir))));

    float shadow = 0.0;
    int2 texSize;
    shadowMap.GetDimensions(texSize.x, texSize.y);
    float2 texelSize = 1.0 / float2(texSize);

    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            float pcfDepth = shadowMap.Sample(projCoords.xy + float2(x, y) * texelSize).r;
            shadow += (currentDepth - bias > pcfDepth) ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;

    return shadow;
}
