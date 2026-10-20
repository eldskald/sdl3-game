#ifndef NORMALS
#define NORMALS

// Normal of the heightmap on the red channel of `tex` at `uv`, from the slopes
// between its 4 neighbors. `strength` scales the slopes, so higher values tilt
// it more. A flat heightmap gives `(0, 0, 1)`, and `x` and `y` point downhill
// in uv space.
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

// Lights `col` by how much the 2D projection of normal `n` points along `dir`,
// in uv coords. Pointing along it brightens up to `highlight`, pointing against
// it darkens up to `shadow`, and a flat normal leaves it as is.
float3 light_normal(
    float3 col, float3 n, float2 dir, float highlight, float shadow) {
    float facing = dot(n.xy, normalize(dir));
    col = lerp(col, 1.0, saturate(facing) * highlight);
    return lerp(col, 0.0, saturate(-facing) * shadow);
}

#endif
