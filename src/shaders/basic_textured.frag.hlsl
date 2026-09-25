// basic_textured.frag.hlsl — Fragment shader with texture, alpha discard, and linear fog.
// Story 4.3.2: Shader Programs [VS1-RENDER-SHADERS]
// FogUniforms cbuffer mirrors FogUniform struct (std140): see MuRendererSDLGpu.cpp.
Texture2D tex : register(t0, space2);
SamplerState s : register(s0, space2);
cbuffer FogUniforms : register(b0, space3)
{
    uint fogEnabled;
    uint alphaDiscardEnabled;
    float alphaThreshold;
    float pad0;
    float fogStart;
    float fogEnd;
    float4 fogColor;
};
struct FSInput
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : TEXCOORD1;
    float fogFactor : TEXCOORD2;
    float3 lightingNormal : TEXCOORD3;
    nointerpolation uint lightingMode : TEXCOORD4;
    nointerpolation float3 lightDirection : TEXCOORD5;
};
static const uint SkinnedLightingFragment = 2u;

float4 main(FSInput input) : SV_Target
{
    const float4 sampledColor = tex.Sample(s, input.uv);
    float4 color = sampledColor * input.color;
    if (alphaDiscardEnabled && color.a <= alphaThreshold) discard;
    if (input.lightingMode == SkinnedLightingFragment)
    {
        const float luminosity = max(dot(normalize(input.lightingNormal), input.lightDirection) * 0.8 + 0.4, 0.2);
        color.rgb = sampledColor.rgb * saturate(input.color.rgb * luminosity);
    }
    // Story 7.9.7: fogFactor=1.0 → no fog (close), fogFactor=0.0 → full fog (far).
    if (fogEnabled) color.rgb = lerp(fogColor.rgb, color.rgb, input.fogFactor);
    return color;
}
