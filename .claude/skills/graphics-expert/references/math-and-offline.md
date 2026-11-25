# Math, offline rendering and procedural techniques

## Contents
- Transforms and projection
- Color science
- Sampling and Monte Carlo
- Ray tracing and path tracing
- Signed distance fields
- Procedural generation
- Numerical pitfalls

## Transforms and projection
- Model -> world -> view -> clip -> NDC -> framebuffer. Clip `w` carries view depth; perspective divide gives NDC.
- **Vulkan clip space**: x right, **y down**, z in 0..1. With glm: `GLM_FORCE_DEPTH_ZERO_TO_ONE` and either flip `proj[1][1] *= -1` or use a negative-height viewport (then front-face winding flips too: set `frontFace` accordingly).
- **Reverse-Z**: map near -> 1, far -> 0, depth compare `GREATER`, clear to 0, optional infinite far plane. Much better float precision; use a float depth buffer.
- Perspective projection (right-handed, zero-to-one): `f = 1/tan(fov/2)`; `P[0][0] = f/aspect`, `P[1][1] = f`, z mapping `z' = far/(near-far)*z + near*far/(near-far)` (standard) or its reversed variant.
- Normal matrix = inverse-transpose of the model's upper 3x3 (only differs from the model matrix under non-uniform scale).
- Quaternions for rotation interpolation (slerp/nlerp), matrices for composition; renormalize drifting quaternions.
- Orthonormal basis from a normal (branchless Duff et al.): useful for sampling hemispheres and tangent frames.
- Reconstruct view-space position from depth using the inverse projection; keep both `inv_proj` and `inv_view_proj` in the per-frame UBO.

## Color science
- sRGB transfer function: piecewise (linear segment under 0.0031308, then `1.055 x^(1/2.4) - 0.055`). 2.2 gamma is an approximation.
- Always shade in **linear** space; convert albedo/emissive textures from sRGB on load (hardware via `_SRGB` formats).
- HDR values are scene-referred; display needs a **tone-mapper** (filmic curves, ACES fit by Narkowicz or Hill, AgX, Khronos PBR Neutral). Apply exposure first in photographic terms (EV).
- Perceptual spaces for grading and gradients: OKLab / OKLCH (interpolate there to avoid muddy mid-tones). Luminance weights for Rec.709: (0.2126, 0.7152, 0.0722).
- HDR10 output: PQ transfer (ST 2084) in Rec.2020; use `VK_EXT_swapchain_colorspace` and a swapchain format such as `A2B10G10R10_UNORM` or `R16G16B16A16_SFLOAT` (scRGB).
- Dither (blue noise or triangular) before quantizing to 8 bits to hide banding in dark gradients.

## Sampling and Monte Carlo
- Estimator: `E[f(X)/p(X)]`. Variance falls like 1/N, so noise halves only at 4x samples.
- **Importance sampling**: draw directions proportional to the integrand. Cosine-weighted hemisphere for diffuse (`pdf = cos/pi`), GGX VNDF sampling for specular, light sampling (area/sphere/solid-angle) for direct lighting. **MIS** (balance or power heuristic) combines BRDF and light sampling robustly.
- Low-discrepancy sequences (Halton, Sobol, R2) and blue-noise dithered offsets give lower perceived noise than white noise; Owen-scrambled Sobol is a strong default.
- Denoise: temporal accumulation (reproject + variance clamp), SVGF / ReBLUR-type filters; ReSTIR for many-light direct illumination.
- Russian roulette to terminate low-throughput paths unbiasedly.

## Ray tracing and path tracing
- **Hardware RT in Vulkan**: `VK_KHR_acceleration_structure`, `VK_KHR_ray_tracing_pipeline` (full pipeline with SBT) or `VK_KHR_ray_query` (trace from any shader, simplest to integrate in a raster engine). Build BLAS per mesh (compact them), TLAS per frame with instance transforms; refit (update) for deforming meshes.
- **Software path**: BVH (SAH build, binned for speed), traverse in compute with a stack, watch divergence. Ray-triangle test: Moller-Trumbore, or watertight (Woop) for robustness.
- **Path tracing loop**: for each bounce: intersect, sample direct light (shadow ray, MIS), sample BRDF for next direction, accumulate `throughput *= f * cos / pdf`, Russian roulette. Accumulate across frames and reset on camera move.
- Offset ray origins along the normal by a scale-aware epsilon (or use the Wächter-Binder offset) to avoid self-intersection.
- Participating media: ray-march or delta-tracking with Henyey-Greenstein phase function.
- Offline use: baking lightmaps, probes, AO, irradiance maps; can run as a compute tool in this engine or an external tool invoked by the asset pipeline.

## Signed distance fields
- SDF returns the distance to the nearest surface; negative inside. Primitives: sphere `length(p) - r`, box, capsule, torus; combine with `min` (union), `max` (intersection), `max(a, -b)` (subtraction), `smin` for blends.
- **Sphere tracing**: `t += sdf(p + rd * t)` until under epsilon or beyond max distance; step count and epsilon scale with distance. Normal by central differences or tetrahedron trick (4 evaluations).
- Soft shadows nearly for free: track `min(k * h / t)` along the shadow ray. AO by sampling the SDF a few steps along the normal.
- Engine uses: mesh SDF / global SDF for soft shadows and AO, font rendering (MSDF), outlines/glows, collision queries, ray-marched volumetric effects.
- 2D SDF text/UI: sample distance texture, `smoothstep` around 0.5 with width from `fwidth` for resolution-independent edges.

## Procedural generation
- **Noise**: value, Perlin/gradient, simplex, Worley; combine as fBm (octaves, lacunarity ~2, gain ~0.5), domain warping for organic looks, ridged/billow variants for mountains and clouds.
- Hash functions on the GPU: PCG (`pcg_hash`) or Wang hash for integer hashes; avoid `sin`-based hashes for large inputs (precision-dependent across vendors).
- Terrain: heightmap from fBm plus hydraulic/thermal erosion in compute; mesh as clipmaps or tessellated patches.
- Textures: tileable noise via periodic domain, triplanar projection to avoid UV stretch, stochastic/hex tiling to break repetition.
- Placement: Poisson-disk sampling or blue-noise; GPU instance generation in compute, culled and drawn indirectly.
- L-systems, wave function collapse, and Voronoi for structures are CPU-side generation problems; hand results to the renderer as meshes or instance buffers.

## Numerical pitfalls
- **Float precision** shrinks away from the origin: large worlds need camera-relative rendering (subtract camera position on CPU in double, upload float offsets).
- Normalize after interpolation (vertex normals shrink), and avoid `normalize(0)` (NaN spreads through bloom/TAA). Add `max(x, epsilon)` and guard divisions.
- `acos`/`sqrt` of values slightly out of range yield NaN; `saturate` the argument.
- NaN/Inf in HDR targets poison temporal history and blur chains: sanitize at the end of each pass (`if (any(isnan(c))) c = 0`).
- Half-float limits: max ~65504; bloom downsample of fireflies needs a Karis average or clamp.
- Mip selection from screen-space derivatives: remember to scale UVs for atlas/virtual textures and use `SampleGrad` in non-uniform flow.
