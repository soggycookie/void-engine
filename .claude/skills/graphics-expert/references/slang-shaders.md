# Slang and shader authoring

## Contents
- Slang essentials
- Compiling and reflection
- Bindings and layout
- Common idioms
- Portability: HLSL / GLSL / WGSL differences
- Shader performance

## Slang essentials
Slang is HLSL-syntax with modules, generics, interfaces, and automatic differentiation. It targets SPIR-V,
DXIL, HLSL, GLSL, Metal, WGSL, and CUDA.

- **Entry points** are marked by attribute: `[shader("vertex")]`, `[shader("fragment")]`, `[shader("compute")]`,
  `[numthreads(8, 8, 1)]` for compute. One `.slang` file can hold several entry points and compile to one SPIR-V module.
- **Parameters declared at global scope** become bindings automatically: `ConstantBuffer<T> ubo;`,
  `Sampler2D tex;` (combined texture+sampler, `tex.Sample(uv)`), `Texture2D<float4> t; SamplerState s;`,
  `RWStructuredBuffer<T> buf;`, `ParameterBlock<T>` (one descriptor set per block, the recommended way to group per-frame / per-material data).
- **System values** use HLSL semantics: `SV_Position`, `SV_Target`, `SV_VertexID`, `SV_InstanceID`,
  `SV_DispatchThreadID`, `SV_GroupThreadID`, `SV_GroupID`, `SV_Depth`. Non-system interpolants can omit semantics (as in `triangle.slang`).
- **Matrix convention**: `mul(M, v)` treats `v` as a column vector. Slang's default matrix layout is
  row-major in memory unless set (`-matrix-layout-column-major` or `-fvk-use-...`). glm is column-major, so either pass
  `-matrix-layout-column-major` to `slangc` or transpose on upload. Decide once. The project's `triangle.slang`
  uses `mul(proj, mul(view, mul(model, v)))`, i.e. column-vector convention, which matches glm's layout if the compile flag matches.
- **Modules and interfaces** let you build material/lighting systems without preprocessor soup:
  ```slang
  interface IBrdf { float3 eval(float3 n, float3 v, float3 l); }
  struct Lambert : IBrdf { float3 albedo; float3 eval(float3 n, float3 v, float3 l) { return albedo / 3.14159265; } }
  float3 shade<B : IBrdf>(B brdf, float3 n, float3 v, float3 l) { return brdf.eval(n, v, l) * max(dot(n, l), 0); }
  ```
  Generics are resolved at compile time (no runtime cost); `import foo;` pulls in module `foo.slang`.
- **Autodiff** (`[Differentiable]`, `bwd_diff(f)`) matters for differentiable rendering; skip unless asked.

## Compiling and reflection
- Typical call: `slangc shader.slang -target spirv -profile spirv_1_5 -entry vertMain -stage vertex -entry fragMain -stage fragment -o shader.spv`.
  Use `-fvk-use-entrypoint-name` or keep one `.spv` with several entry points; with one module, create each `VkPipelineShaderStageCreateInfo` using `pName` = the entry-point name.
- `-matrix-layout-column-major`, `-O2`, `-g` (debug info for RenderDoc), `-I` for module search paths.
- In this repo the CMake `all_shaders` target runs `slangc` over `resource/base/shader/slang/*.slang`. After adding a shader, re-run configure if the glob does not pick it up.
- Vulkan binding control: `[[vk::binding(binding, set)]]`, `[[vk::push_constant]]`, `[[vk::location(n)]]`. Without them Slang assigns bindings in declaration order, so changing the shader's declaration order changes the descriptor layout. Either pin bindings explicitly or generate the layout from Slang's reflection API (`slang::ProgramLayout`).
- Spec constants: `[[vk::constant_id(0)]] const int kQuality = 1;`.

## Bindings and layout
- **std140/std430 vs Slang**: `ConstantBuffer<T>` follows std140-like rules (vec3 aligned to 16, array stride rounded to 16). `StructuredBuffer<T>` uses std430-like scalar rules. A CPU struct that mismatches is the #1 cause of "garbage in buffer". Mirror structs with explicit padding and `static_assert(sizeof(...))`/`offsetof` checks, using `float4`-friendly packing.
- Push constants: 128 bytes guaranteed; good for per-draw index/material id.
- **Bindless**: `Texture2D textures[]` with `VK_EXT_descriptor_indexing` (core in 1.2: `runtimeDescriptorArray`, `descriptorBindingPartiallyBound`, `descriptorBindingUpdateAfterBind`). Index with `NonUniformResourceIndex(i)` when `i` is not dynamically uniform across the wave. In Slang, `DescriptorHandle<Texture2D>` is the modern spelling.
- Vertex inputs: declare with `VSInput` struct; the `VkVertexInputAttributeDescription` locations follow member order (or `[[vk::location]]`). Check `offset`/`format` against the C++ `Vertex`.

## Common idioms
- **Full-screen pass**: no vertex buffer, `SV_VertexID` generates one oversized triangle:
  `float2 uv = float2((id << 1) & 2, id & 2); pos = float4(uv * 2 - 1, 0, 1);` (flip Y for Vulkan if sampling a render target).
- **Normal from depth / reconstruct world position**: `ndc = float3(uv * 2 - 1, depth)`; `p = mul(invViewProj, float4(ndc, 1))`; `p /= p.w`. Vulkan depth is 0..1 and clip Y points down, so match your projection matrix (glm: define `GLM_FORCE_DEPTH_ZERO_TO_ONE`, and flip `proj[1][1]` or use negative viewport height).
- **Octahedral normal encoding** (2 channels instead of 3) for G-buffers; **tangent frame** from derivative (`ddx/ddy`, cotangent frame) when no tangents exist.
- **sRGB**: use `*_SRGB` image formats so sampling/writing converts for you; only hand-code `pow(x, 2.2)` or the exact piecewise curve when working on UNORM storage.
- **Compute dispatch**: `groupCount = (size + group - 1) / group`; guard with `if (any(id.xy >= size)) return;`. Common sizes: 8x8 (2D), 64 or 256 (1D). Use `groupshared` + `GroupMemoryBarrierWithGroupSync()` for reductions/blur tiles.
- **Derivatives in non-uniform control flow are undefined**; compute `Sample` in uniform flow or use `SampleLevel`/`SampleGrad`.
- **Branching**: divergence costs only if lanes of one wave (32 NVIDIA / 32 or 64 AMD / varies Intel) disagree. Prefer `select`/`lerp` for tiny branches, real branches for large uniform-ish work.

## Portability notes
- HLSL -> Slang: usually compiles unchanged. Differences: Slang `Sampler2D` combined type, `ParameterBlock`, `import`, `[shader(...)]` attributes instead of `-E` flags.
- GLSL -> Slang: `texture(sampler2D, uv)` -> `tex.Sample(uv)`, `mix` -> `lerp`, `fract` -> `frac`, `vec3` -> `float3`, matrices are `mat * vec` -> `mul(mat, vec)` with swapped operand sense relative to how it was uploaded.
- D3D11 (legacy backend in `asset/shader/platform/d3d11/`) uses HLSL via `fxc`/`dxc`. Shared logic can live in a Slang module and be targeted at `-target hlsl`.
- WGSL/Metal: Slang targets them; the main gotchas are uniform buffer alignment and NDC Y/Z conventions.

## Shader performance
- Count what dominates: texture fetches (cache misses, anisotropic filtering), interpolants, registers (occupancy), ALU, memory writes. Use vendor tools (Nsight Graphics, RGP, Xcode GPU) to see occupancy and bottleneck percentages instead of guessing.
- Use `half`/`min16float` only when tested; many desktop GPUs compute at 32-bit anyway.
- Hoist loop-invariant work to the vertex stage or CPU; avoid `pow` with constant exponent where multiplication works; use `rsqrt`, `saturate`, `mad` freely (compiler fuses).
- Early-Z needs: no `discard`/depth write in the pixel shader, or declare `[earlydepthstencil]`. Alpha-tested foliage should go in a separate pass after opaque work.
- Avoid dynamic indexing into per-thread arrays (spills to local memory). Use constants or `groupshared`.
