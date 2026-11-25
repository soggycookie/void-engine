# Real-time rendering techniques

For each technique: what it does, the core idea, the usual pitfalls. Always give the user the trade-off
between at least two options before implementing.

## Contents
- Pipeline choice
- Lighting and materials (PBR)
- Shadows
- Ambient light and global illumination
- Post-processing
- Anti-aliasing
- Transparency, volumetrics and special surfaces
- Geometry, culling and LOD
- Animation and particles
- Visual-goal quick map

## Pipeline choice
| Pipeline | Good when | Cost |
|---|---|---|
| **Forward** | few lights, MSAA wanted, transparency heavy, mobile | cost = lights x overdraw |
| **Forward+ / clustered forward** | many lights, keep MSAA and materials flexible | light-culling compute pass, light lists buffer |
| **Deferred** | many lights, many screen-space effects, simple materials | fat G-buffer bandwidth, no MSAA, separate transparency pass |
| **Visibility buffer** | very high triangle counts, GPU-driven | per-pixel attribute fetch in shading pass |

Default recommendation for a new engine: clustered forward (or plain forward first) with a depth prepass.
G-buffer layout (deferred) example: RT0 albedo.rgb + AO, RT1 octahedral normal + roughness + metalness, RT2 emissive (HDR), depth for position.

## Lighting and materials (PBR)
- **Rendering equation** (what we approximate): outgoing light = emitted + integral over the hemisphere of BRDF x incoming radiance x cos(theta).
- **Metallic-roughness workflow**: `albedo`, `metallic`, `roughness`, `normal`, `ao`, `emissive`. `F0 = lerp(0.04, albedo, metallic)`, diffuse color = `albedo * (1 - metallic)`.
- **Cook-Torrance specular**: `D * G * F / (4 * NdotL * NdotV)` with GGX `D` (alpha = roughness^2), Smith height-correlated `G`, Schlick `F`. Diffuse: Lambert (`albedo / pi`), or Burley/Disney if extra softness at grazing is wanted.
- Clamp roughness to a small minimum (~0.045) to avoid zero-size highlights; `NdotV` with `max(..., 1e-4)`.
- **Light types**: directional (sun), point/spot with physical falloff `1/d^2` windowed to a radius, area lights via LTC (Linearly Transformed Cosines) for realistic highlights.
- **Image-based lighting**: diffuse irradiance cubemap or spherical harmonics (L2, 9 coefficients); specular prefiltered cubemap per roughness mip plus a BRDF LUT (split-sum). Pre-process offline or at load with compute.
- **Normal mapping**: tangent-space normals (UNORM, remap `n*2-1`, renormalize); build TBN from mesh tangents (MikkTSpace) or derivatives. Green-channel convention differs (OpenGL vs DirectX).
- Energy conservation and specular antialiasing (Toksvig or normal-variance roughness boost) remove shimmering on bumpy surfaces.
- Stylized goals (toon, cel, painterly): quantize N.L into bands via a ramp texture, rim light `pow(1 - NdotV, k)`, inverted-hull or edge-detection outlines (depth/normal/ID Sobel in post).

## Shadows
- **Shadow mapping**: render depth from the light, compare in the lit pass. Needs bias (constant + slope-scaled via `vkCmdSetDepthBias`) and normal-offset to fight acne; too much bias gives peter-panning.
- **Cascaded shadow maps (CSM)** for the sun: split the camera frustum (practical split scheme: blend of linear and logarithmic, 3-4 cascades), fit each cascade's orthographic projection to the slice's bounding sphere and **snap to texel size** to stop shimmer when the camera moves. Blend between cascades at boundaries.
- **Filtering**: PCF with `sampler2DShadow` + Poisson/rotated disk; PCSS for contact-hardening penumbra (blocker search -> penumbra size -> filter); VSM/EVSM/MSM are filterable but light-bleed.
- Point lights: cube shadow map (6 faces) or dual-paraboloid; budget carefully, cache static lights.
- **Ray-traced shadows** exist when `VK_KHR_ray_query` is available: perfect contact shadows, needs BLAS/TLAS.
- Screen-space contact shadows add detail for small occluders at low cost (short ray march in depth buffer).
- Use reverse-Z + `D32_SFLOAT` and a comparison sampler with `compareOp = GREATER_OR_EQUAL` when reverse-Z is on.

## Ambient light and global illumination
- Constant ambient < hemisphere ambient < SH/probe lighting < baked lightmaps < dynamic GI.
- **SSAO / GTAO / HBAO**: darkens crevices from the depth buffer at half resolution + blur (bilateral/depth-aware). GTAO has the better physical basis. Pair with multi-bounce approximation for bright albedo.
- **Light probes**: grid of SH probes, trilinear blend; reflection probes (box-projected cubemaps) for local specular. Baking is offline or via path tracing.
- **Dynamic GI options**: voxel cone tracing (VXGI, heavy memory), DDGI (ray-traced irradiance probes, needs RT), screen-space GI (SSGI, cheap but view-dependent), Radiance Cascades (screen-space/2D friendly), Lumen-style surface cache (large engineering effort). Recommend by hardware floor and engineering budget.
- **SSR**: ray-march depth buffer using the reflection vector (hierarchical Z for speed), fall back to probes where the ray exits the screen or fails; roughness-aware blur/cone.

## Post-processing
Order matters. Typical HDR chain, all in linear space:
1. Scene color (R16G16B16A16_SFLOAT) -> exposure (auto-exposure via luminance histogram compute, EV100) -> **bloom** (downsample/upsample chain à la "Call of Duty: Advanced Warfare" with a 13-tap filter, then add with low intensity) -> **tonemap** (ACES approximations, AgX, Khronos PBR Neutral, or Reinhard-extended; pick by look) -> color grade (LUT 32^3) -> film grain/vignette -> **sRGB encode** -> UI on top (UI in display space).
- **Depth of field**: circle-of-confusion from depth, gather in half-res with bokeh kernel; scatter-as-gather for quality.
- **Motion blur**: per-pixel velocity buffer (written in the geometry pass) -> tile-max -> neighbor-max -> blur. Needs previous-frame matrices.
- **Lens flare/chromatic aberration/vignette**: cheap and effective if subtle; overuse reads as amateur.
- **Fog**: analytic exponential height fog in the lit shader; **volumetric fog** via froxel grid (light scattering injected + integrated by compute, temporal reprojection).
- Combine passes to save bandwidth; use compute with `groupshared` for blurs and reductions.

## Anti-aliasing
- MSAA: great for geometry edges in forward, no help for shader aliasing, costly with deferred.
- FXAA/SMAA: post filters, cheap, a bit soft.
- **TAA**: jitter projection (Halton 2,3), motion vectors, history reprojection, neighborhood clamp (variance clipping) to limit ghosting, sharpen after. Also gives free supersampling for SSAO/SSR/volumetrics. Jitter must be removed from motion vectors and the previous frame's jitter accounted for. Reset history on camera cuts.
- Upscalers (FSR, DLSS, XeSS) need exactly the TAA inputs: jittered color, depth, motion vectors, exposure.

## Transparency, volumetrics and special surfaces
- Alpha blending needs back-to-front sort; for robust option use **weighted blended OIT** (cheap, approximate) or per-pixel linked lists (exact, memory heavy). Alpha-tested foliage: alpha-to-coverage with MSAA, or dithered + TAA.
- **Water**: Gerstner/FFT displacement, normal map scrolling, Fresnel blend of reflection (SSR/planar/probe) and refraction (copy of opaque scene color, depth-based absorption + foam at intersection).
- **Terrain**: heightmap clipmaps or CDLOD, splat/triplanar blending with height-based blending, virtual texturing for scale.
- **Skin/subsurface**: separable SSS blur, thickness-based translucency; **hair**: Kajiya-Kay or Marschner shading, alpha-tested strips with OIT.
- **Decals**: project a box onto the depth buffer, write albedo/normal into G-buffer (deferred) or blend in forward.
- **Sky/atmosphere**: precomputed Bruneton/Hillaire atmosphere LUTs (transmittance, multi-scattering, sky-view) in a few compute passes; clouds by ray-marched noise-based density with temporal upsampling.
- **Procedural noise in shaders**: value/gradient/simplex, Worley (cellular), blue-noise textures for dithering and sampling.

## Geometry, culling and LOD
- CPU frustum culling with AABB/sphere against 6 planes (extract from view-proj, normalize).
- GPU culling in compute: frustum + Hi-Z occlusion (two-phase: draw last frame's visible, test the rest, draw newly visible) writing indirect draw args.
- LOD: discrete LODs with screen-size thresholds and dithered crossfade; meshlets + cluster hierarchy for continuous LOD (Nanite-style) when mesh shaders are available. Tessellation is niche today.
- Instancing for repeated meshes; sort draws by pipeline, material, depth.
- Mesh optimization offline: `meshoptimizer` for vertex cache, overdraw, vertex fetch, quantization, meshlet building.

## Animation and particles
- **Skinning**: bone matrices (<= 4 influences per vertex, weights normalized), uploaded per frame to a storage buffer; dual-quaternion skinning avoids candy-wrapper; do it in the vertex shader or a compute pre-pass if you need skinned data for shadows/raytracing.
- **Blend shapes** as sparse deltas in a storage buffer.
- **GPU particles**: SoA storage buffers, compute update (emit via atomic counter / append list, simulate, compact), indirect draw of billboards or meshes; soft particles fade by depth difference; sort only if alpha-blended.

## Visual-goal quick map
| User says | Start with |
|---|---|
| "looks flat" | PBR with normal maps, IBL, shadows, AO |
| "no depth / floating objects" | contact shadows, SSAO/GTAO, correct shadow bias |
| "too dark / washed out / banding" | linear workflow, tonemap, auto-exposure, dither at 8-bit output |
| "glow" | HDR emissive + bloom |
| "shimmering / crawling edges" | TAA or specular AA, mipmaps, anisotropic filtering |
| "realistic outdoor" | CSM + atmosphere + exponential fog + SSAO + SSR water |
| "cartoon" | toon ramp + outlines + flat shadows, avoid PBR specular |
| "retro / pixel" | low-res render target, nearest upscale, palette LUT, dithering |
| "lots of lights" | clustered forward/deferred with compute light culling |
| "mirror / reflections" | planar (single mirror), SSR + probes, or RT reflections |
