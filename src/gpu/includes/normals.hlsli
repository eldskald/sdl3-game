#ifndef NORMALS
#define NORMALS

float3 height_to_normal(Texture2D<float4> tex,
                        SamplerState s,
                        float2 uv,
                        float strength) {
    uint width, height;
    tex.GetDimensions(width, height);
    float2 dx = float2(1.0 / width, 0.0);
    float2 dy = float2(0.0, 1.0 / height);

    float u = tex.Sample(s, uv - dy).r;
    float l = tex.Sample(s, uv - dx).r;
    float d = tex.Sample(s, uv + dy).r;
    float r = tex.Sample(s, uv + dx).r;

    return normalize(float3((l - r) * strength, (u - d) * strength, 2.0));
}

float3 light_normal(
    float3 col, float3 n, float2 dir, float highlight, float shadow) {
    float facing = dot(n.xy, normalize(dir));
    col = lerp(col, 1.0, saturate(facing) * highlight);
    return lerp(col, 0.0, saturate(-facing) * shadow);
}

#endif
