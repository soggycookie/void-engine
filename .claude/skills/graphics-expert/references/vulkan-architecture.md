# Vulkan architecture, sync and memory

## Contents
- Mental model
- Frame structure
- Synchronization (the part people get wrong)
- Memory and resources
- Descriptors and pipelines
- Renderer architecture
- Performance
- Other APIs in one paragraph each

## Mental model
Vulkan records commands into command buffers, submits them to queues, and the GPU executes them in a
partially ordered, overlapping fashion. **Nothing is ordered unless you order it**: barriers, semaphores and
fences are the entire ordering contract. The driver does no hazard tracking. Layouts describe how texels are
arranged for a given use; transitions are a barrier side effect.

Baseline for this engine: Vulkan 1.3 core features, `dynamicRendering`, `synchronization2`, `bufferDeviceAddress`,
descriptor indexing, timeline semaphores, VMA for allocations.

## Frame structure
```
per frame-in-flight (N = 2 or 3):  command pool/buffer, fence, UBO/ring slice, descriptor set(s)
per swapchain image:               render_finished semaphore (present waits on it)
per frame-in-flight:               acquire semaphore

wait fence[f] -> acquire(sem acquire[f]) -> reset fence[f] -> record -> submit(wait acquire[f], signal render_finished[image], fence[f]) -> present(wait render_finished[image])
```
- Do not `vkQueueWaitIdle` per frame; it removes CPU/GPU overlap.
- Handle `VK_ERROR_OUT_OF_DATE_KHR` (recreate) and `VK_SUBOPTIMAL_KHR`; skip rendering when extent is 0x0.
- Prefer timeline semaphores for GPU-GPU and CPU-GPU waits in new code; binary semaphores are still needed for swapchain acquire/present.
- Use `vkCmdSetViewport/Scissor` as dynamic state so resize does not rebuild pipelines; flip Y with a negative viewport height (Vulkan 1.1+) if you want OpenGL-style up.
- Details specific to this repo's frame loop are in the `review-vulkan` skill.

## Synchronization
Each barrier is: *everything in (srcStage, srcAccess) happens-before everything in (dstStage, dstAccess)* for the resources named.

- **Think in producer -> consumer pairs.** Ask: who wrote it last (stage, access, layout), who reads it next?
  Examples:
  - Compute writes storage image, fragment samples it: src `COMPUTE_SHADER`/`SHADER_STORAGE_WRITE` layout `GENERAL`, dst `FRAGMENT_SHADER`/`SHADER_SAMPLED_READ` layout `SHADER_READ_ONLY_OPTIMAL`.
  - Color attachment then sampled: src `COLOR_ATTACHMENT_OUTPUT`/`COLOR_ATTACHMENT_WRITE`, dst `FRAGMENT_SHADER`/`SHADER_SAMPLED_READ`.
  - Depth to sampled shadow map: src `LATE_FRAGMENT_TESTS`/`DEPTH_STENCIL_ATTACHMENT_WRITE` (and early for both), dst `FRAGMENT_SHADER`/`SHADER_SAMPLED_READ`.
  - Swapchain: `UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL` (dst stage `COLOR_ATTACHMENT_OUTPUT`, matching the acquire semaphore wait stage) then `-> PRESENT_SRC_KHR` (dst stage `NONE`).
  - Transfer upload then use: src `TRANSFER`/`TRANSFER_WRITE`, dst the consuming stage.
- **Layout tracking**: keep a per-image (and per-mip/layer when needed) tracked state in the renderer; barrier helpers take `(image, new_state)` and derive old state, so call sites cannot lie about `oldLayout`.
- **WAR hazards** need execution barriers too (reading before overwrite); `srcAccess = 0` is valid for them.
- `ALL_COMMANDS` barriers are correct but serializing; use as a first draft and tighten when profiling.
- **Batching**: gather barriers and emit one `vkCmdPipelineBarrier2` with many entries.
- Queue family ownership transfer only when using separate queues with `EXCLUSIVE` sharing.
- Always run with `VK_LAYER_KHRONOS_validation` and enable **synchronization validation** (`VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT`) while developing barriers. It catches most hazards.

## Memory and resources
- **VMA**: `VMA_MEMORY_USAGE_AUTO` + flags. Staging: `HOST_ACCESS_SEQUENTIAL_WRITE` + `TRANSFER_SRC`. Per-frame UBO/dynamic data: `HOST_ACCESS_SEQUENTIAL_WRITE | MAPPED`. GPU-only: no host flags. Readback: `HOST_ACCESS_RANDOM`. Use `vmaFlushAllocation` if memory is non-coherent.
- Upload path: staging buffer -> `vkCmdCopyBufferToImage` -> barrier -> sampled. Batch uploads into one command buffer and one fence; free staging only after that fence signals. With resizable BAR, small hot data can be written to device-local host-visible memory directly.
- **Suballocate** buffers: one big vertex/index buffer plus offsets (or `bufferDeviceAddress` pointers) instead of one buffer per mesh. Fewer binds, fewer allocations (drivers cap allocation count, ~4096).
- **Mipmaps**: generate with `vkCmdBlitImage` (check `FORMAT_FEATURE_BLIT_*` and linear filter support) or a compute downsample; each level needs its own transition.
- **Formats**: color `R8G8B8A8_SRGB` for albedo, `UNORM` for data, `R16G16B16A16_SFLOAT` for HDR targets, `B10G11R11_UFLOAT_PACK32` for cheap HDR, `D32_SFLOAT` for depth (reverse-Z), `BC7`/`BC5`/`BC1` compressed (KTX2/Basis) for shipping textures.
- Transient attachments (`LAZILY_ALLOCATED` + `TRANSIENT_ATTACHMENT`) matter on tiled GPUs; on desktop they act as normal memory.
- Destroy order: wait idle (or per-resource fence), then destroy views before images, VMA objects before allocator, allocator before device. Deferred deletion queues (per frame-in-flight) avoid destroying in-use resources during resize.

## Descriptors and pipelines
- Pick one binding model and stay with it:
  1. **Classic sets** (per-frame set 0, per-material set 1, per-draw push constants): simplest to reason about.
  2. **Bindless descriptor indexing**: one big texture array + storage buffers, materials referenced by index, great for GPU-driven rendering.
  3. **Buffer device address / descriptor buffer**: pointer-based, fewest API objects, most DIY.
- Descriptor pool size must cover `frames * sets`; use `UPDATE_AFTER_BIND` for bindless.
- `VkPipelineCache` saved to disk cuts start-up; pipeline creation is the main hitch source, so build at load time or async, never mid-frame. Use `VK_EXT_graphics_pipeline_library` or shader objects (`VK_EXT_shader_object`) if variant count explodes.
- Pipeline layout must be compatible with the shader's reflection: set/binding/type/stage flags/array size.
- Dynamic rendering: `VkPipelineRenderingCreateInfo` formats must match `vkCmdBeginRendering` attachments.
- MSAA: resolve attachments in the render pass; `sampleShading` costs. For deferred, prefer TAA or SMAA.

## Renderer architecture
Grow in this order; do not skip ahead:
1. Single triangle -> textured mesh with depth -> multiple objects (push constants / dynamic UBO).
2. A `Renderer` that owns frames-in-flight, a deletion queue, an upload context, and a barrier/layout tracker.
3. Material system (shader + pipeline + descriptor layout cache), mesh/texture asset loading via `ResourceSystem`.
4. **Render graph / frame graph**: declare passes with read/write resources; the graph computes ordering, barriers, layout transitions, transient aliasing. Worth it once there are more than ~5 passes with resource sharing.
5. GPU-driven rendering: indirect draws (`vkCmdDrawIndexedIndirectCount`), compute frustum/occlusion culling, mesh shaders (`VK_EXT_mesh_shader`) for meshlet pipelines.
6. Multi-threaded recording: one command pool per thread per frame, secondary command buffers or one primary per job (the JobSystem here fits). Never share a command pool across threads.

Keep the renderer API-neutral boundary thin: `renderer_api.h` exists for D3D11 vs Vulkan; avoid leaking `Vk*` types above it unless the legacy backend is dropped.

## Performance
- Measure with timestamp queries (`vkCmdWriteTimestamp2`) per pass and a GPU capture tool. Look at: pass time, overdraw, bandwidth, occupancy.
- Reduce state changes (sort by pipeline, then descriptor set, then mesh), instance repeated meshes, batch draws.
- Use a depth prepass or front-to-back sort when the pixel shader is heavy and overdraw is high.
- Avoid read-after-write chains across many tiny dispatches; fuse passes (for example tonemap + bloom composite).
- Async compute only pays off when graphics is bandwidth-bound and compute is ALU-bound; it needs a separate queue family and ownership barriers.

## Other APIs in brief
- **D3D12** mirrors Vulkan (explicit barriers via enhanced barriers, descriptor heaps, root signatures instead of pipeline layouts, PSOs).
- **D3D11** (legacy backend): immediate context, driver tracks hazards; simple but opaque, no explicit memory. Resources bound via `PSSetShaderResources` etc.
- **OpenGL**: global state machine; DSA (4.5) and `ARB_bindless_texture` modernize it.
- **Metal**: explicit like Vulkan but with automatic hazard tracking available (`MTLHazardTrackingMode`) and tile shading on Apple GPUs.
- **WebGPU**: safe subset, bind groups, WGSL, no manual barriers; limits are strict (uniform buffer alignment 256 bytes by default).
