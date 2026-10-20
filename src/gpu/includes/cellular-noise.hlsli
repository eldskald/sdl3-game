//
// Description : GPU cellular (Voronoi/Worley) noise in 3D and 4D, with
//               fractal sums of F1 and F2 - F1.
//      Origin : Unity project by Justin (Scrawk).
//               https://github.com/Scrawk/GPU-Voronoi-Noise
//

#ifndef CELLULAR_NOISE
#define CELLULAR_NOISE

// 1/7
static const float CELLULAR_K = 0.142857142857;
// 3/7
static const float CELLULAR_KO = 0.428571428571;

// GLSL style modulo, unlike HLSL's fmod it's always positive for positive y.
float3 cellularMod(float3 x, float y) {
    return x - y * floor(x / y);
}

float4 cellularMod(float4 x, float y) {
    return x - y * floor(x / y);
}

// Permutation polynomial: (34x^2 + x) mod 289
float3 cellularPermute(float3 x) {
    return cellularMod((34.0 * x + 1.0) * x, 289.0);
}

// Returns the squared distances to the closest (F1) and second closest (F2)
// feature points. `jitter` goes from 0.0 (regular grid) to 1.0 (fully random).
float2 cellular(float3 P, float jitter) {
    float3 Pi = cellularMod(floor(P), 289.0);
    float3 Pf = frac(P);
    float3 oi = float3(-1.0, 0.0, 1.0);
    float3 of = float3(-0.5, 0.5, 1.5);
    float3 px = cellularPermute(Pi.x + oi);
    float3 py = cellularPermute(Pi.y + oi);

    float3 p, ox, oy, oz, dx, dy, dz;
    float2 F = 1e6;

    [unroll] for (int i = 0; i < 3; i++) {
        [unroll] for (int j = 0; j < 3; j++) {
            p = cellularPermute(px[i] + py[j] + Pi.z + oi); // pij1, pij2, pij3

            ox = frac(p * CELLULAR_K) - CELLULAR_KO;
            oy = cellularMod(floor(p * CELLULAR_K), 7.0) * CELLULAR_K -
                 CELLULAR_KO;

            p = cellularPermute(p);

            oz = frac(p * CELLULAR_K) - CELLULAR_KO;

            dx = Pf.x - of[i] + jitter * ox;
            dy = Pf.y - of[j] + jitter * oy;
            dz = Pf.z - of + jitter * oz;

            // dij1, dij2 and dij3, squared
            float3 d = dx * dx + dy * dy + dz * dz;

            // Find lowest and second lowest distances
            [unroll] for (int n = 0; n < 3; n++) {
                if (d[n] < F[0]) {
                    F[1] = F[0];
                    F[0] = d[n];
                } else if (d[n] < F[1]) {
                    F[1] = d[n];
                }
            }
        }
    }

    return F;
}

float2 cellular(float4 P, float jitter) {
    float4 Pi = cellularMod(floor(P), 289.0);
    float4 Pf = frac(P);
    float3 oi = float3(-1.0, 0.0, 1.0);
    float3 of = float3(-0.5, 0.5, 1.5);
    float3 px = cellularPermute(Pi.x + oi);
    float3 py = cellularPermute(Pi.y + oi);
    float3 pz = cellularPermute(Pi.z + oi);

    float3 p, ox, oy, oz, ow, dx, dy, dz, dw, d;
    float2 F = 1e6;

    [unroll] for (int i = 0; i < 3; i++) {
        [unroll] for (int j = 0; j < 3; j++) {
            [unroll] for (int k = 0; k < 3; k++) {
                // pijk1, pijk2, pijk3
                p = cellularPermute(px[i] + py[j] + pz[k] + Pi.w + oi);

                ox = frac(p * CELLULAR_K) - CELLULAR_KO;
                oy = cellularMod(floor(p * CELLULAR_K), 7.0) * CELLULAR_K -
                     CELLULAR_KO;

                p = cellularPermute(p);

                oz = frac(p * CELLULAR_K) - CELLULAR_KO;
                ow = cellularMod(floor(p * CELLULAR_K), 7.0) * CELLULAR_K -
                     CELLULAR_KO;

                dx = Pf.x - of[i] + jitter * ox;
                dy = Pf.y - of[j] + jitter * oy;
                dz = Pf.z - of[k] + jitter * oz;
                dw = Pf.w - of + jitter * ow;

                // dijk1, dijk2 and dijk3, squared
                d = dx * dx + dy * dy + dz * dz + dw * dw;

                // Find the lowest and second lowest distances
                [unroll] for (int n = 0; n < 3; n++) {
                    if (d[n] < F[0]) {
                        F[1] = F[0];
                        F[0] = d[n];
                    } else if (d[n] < F[1]) {
                        F[1] = d[n];
                    }
                }
            }
        }
    }

    return F;
}

// Fractal sum of F1.
float fBm_F0(float3 p, int octaves, float frequency, float lacunarity,
             float gain, float jitter) {
    float freq = frequency, amp = 0.5;
    float sum = 0.0;
    for (int i = 0; i < octaves; i++) {
        float2 F = cellular(p * freq, jitter) * amp;

        sum += 0.1 + sqrt(F[0]);

        freq *= lacunarity;
        amp *= gain;
    }
    return sum;
}

float fBm_F0(float4 p, int octaves, float frequency, float lacunarity,
             float gain, float jitter) {
    float freq = frequency, amp = 0.5;
    float sum = 0.0;
    for (int i = 0; i < octaves; i++) {
        float2 F = cellular(p * freq, jitter) * amp;

        sum += 0.1 + sqrt(F[0]);

        freq *= lacunarity;
        amp *= gain;
    }
    return sum;
}

// Fractal sum of F2 - F1, gives cell borders.
float fBm_F1_F0(float3 p, int octaves, float frequency, float lacunarity,
                float gain, float jitter) {
    float freq = frequency, amp = 0.5;
    float sum = 0.0;
    for (int i = 0; i < octaves; i++) {
        float2 F = cellular(p * freq, jitter) * amp;

        sum += 0.1 + sqrt(F[1]) - sqrt(F[0]);

        freq *= lacunarity;
        amp *= gain;
    }
    return sum;
}

float fBm_F1_F0(float4 p, int octaves, float frequency, float lacunarity,
                float gain, float jitter) {
    float freq = frequency, amp = 0.5;
    float sum = 0.0;
    for (int i = 0; i < octaves; i++) {
        float2 F = cellular(p * freq, jitter) * amp;

        sum += 0.1 + sqrt(F[1]) - sqrt(F[0]);

        freq *= lacunarity;
        amp *= gain;
    }
    return sum;
}

#endif
