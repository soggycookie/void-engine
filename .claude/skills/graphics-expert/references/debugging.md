# Debugging and profiling graphics

Approach: bisect the pipeline. Prove each stage's output with a capture or a debug view before theorizing.
You cannot see the screen, so tell the user exactly what to inspect and what they should see, and ask them to report back.

## Contents
- First steps
- Symptom to cause table
- Validation layer messages
- RenderDoc workflow
- GPU hangs and device lost
- Profiling

## First steps
1. Enable validation (`VK_LAYER_KHRONOS_validation`) with debug messenger, plus synchronization validation and, for suspicious cases, GPU-assisted and best-practices validation. Fix the first message before the rest; later ones are often consequences.
2. Name objects (`VK_EXT_debug_utils`: `vkSetDebugUtilsObjectNameEXT`, labels around passes) so captures are readable.
3. Capture one frame in RenderDoc and walk the event list.
4. Simplify: output a constant color, then UVs, then normals, then texture sample, then lighting, one term at a time.

## Symptom to cause
| Symptom | Likely causes (check in this order) |
|---|---|
| **Black / blank screen** | Nothing submitted or presented; swapchain image never transitioned to `COLOR_ATTACHMENT_OPTIMAL`/`PRESENT_SRC`; viewport/scissor zero or wrong; culled by winding order (`frontFace` vs flipped viewport) or by `cullMode`; depth test fails (clear value vs compare op, reverse-Z mismatch); MVP matrix wrong (check in RenderDoc mesh output tab); vertex buffer not bound or wrong offset; shader outputs alpha 0 with blending enabled |
| **Only background color** | Draw happens but triangles are clipped: check vertex output position in RenderDoc "Mesh Output"; check near/far and `w` |
| **Upside-down image** | Vulkan Y-down clip space; flip projection or viewport |
| **Inside-out mesh** | Winding/cull mismatch after Y flip, handedness of the asset |
| **Texture black or garbage** | Layout not `SHADER_READ_ONLY_OPTIMAL` when sampled; upload finished after use (missing barrier or fence); wrong format (sRGB vs UNORM), wrong mip count in view/sampler, descriptor not updated; staging freed early |
| **Colors too dark/bright** | Double or missing sRGB conversion; UNORM swapchain with linear output but no encode; albedo uploaded as UNORM |
| **Flicker, tearing of geometry** | Per-frame resource reused while GPU still reading (no fence/ring buffer); missing barrier between passes; semaphore reuse |
| **Works then breaks on resize** | Swapchain/depth/framebuffer-sized resources not recreated; stale image views; extent 0x0 on minimize; per-image semaphores not recreated when image count changes |
| **Z-fighting** | Insufficient depth precision (use reverse-Z + float depth), near plane too small, coplanar geometry, polygon offset |
| **Shadow acne / peter-panning** | Bias too low / too high; use slope-scaled bias + normal offset; check shadow-map depth range and cascade fitting |
| **NaN black squares or blown pixels** | Division by zero, `normalize(0)`, `pow` of negative, uninitialized storage; propagates through bloom/TAA |
| **Shimmering on camera motion** | No mips/anisotropy, specular aliasing, shadow cascade not texel-snapped, TAA jitter not matched |
| **Constant buffer values wrong** | Struct alignment mismatch (vec3 padding), matrix layout/transposition, upload to wrong frame's buffer, non-coherent flush missing |
| **Compute pass writes nothing** | Dispatch size 0 or wrong, descriptor type mismatch (`STORAGE_IMAGE` needs `GENERAL`), missing `VK_IMAGE_USAGE_STORAGE_BIT`, out-of-bounds guard wrong |
| **Slang compile or link errors** | Entry-point name/stage mismatch, missing `[shader(...)]`, binding conflicts, matrix layout flag; run `slangc` manually to read the full message |

## Validation layer messages
- Read the `VUID-...` identifier; search the Vulkan spec for it, it states the rule.
- Image layout errors (the message names the expected and actual layout): your tracked layout is out of sync with the real layout.
- `SYNC-HAZARD-WRITE-AFTER-READ` etc. (sync validation): the message lists both accesses (command, stage, access); add the missing barrier between exactly those two.
- `VUID-vkQueueSubmit-pSignalSemaphores-...` / semaphore in use: reusing a semaphore while a previous signal/wait is pending, usually per-frame vs per-image indexing mixups.
- Object-leak messages at shutdown mean the destroy order/symmetry is broken, check the `shutdown()` path.
- Silence is not proof of correctness: validation misses logic bugs (wrong matrices, wrong data).

## RenderDoc workflow
- Event Browser: confirm the draw exists and has the expected counts.
- Texture Viewer: inspect inputs/outputs of that draw, use the pixel history on a wrong pixel to see which draws touched it and why they passed/failed.
- Mesh Viewer: compare VS input vs VS output vs GS/post-VS; reveals wrong MVP, bad indices, bad vertex layout.
- Pipeline State: confirm viewport, scissor, cull mode, blend, depth state, bound descriptors/formats.
- Shader debugger (needs `-g` and non-optimized or with debug info via Slang; SPIR-V with `NonSemantic.Shader.DebugInfo`): step a pixel.
- Buffer viewer with the right struct layout to confirm UBO content.
- Programmatic captures: `RENDERDOC_API` in-app trigger, or launch from RenderDoc's UI with the exe `bin/Debug/<name>.exe` (working dir must contain `resource/` and `asset/`).

## GPU hangs and device lost
- `VK_ERROR_DEVICE_LOST` usually = out-of-bounds buffer/descriptor access, infinite shader loop, or a timeout (TDR on Windows ~2s). Find the offending submit by bisecting passes.
- Enable `robustBufferAccess2`/`robustImageAccess2` while debugging to turn OOB into zeros.
- Use `VK_NV_device_diagnostic_checkpoints` / AMD buffer markers, Nsight Aftermath, or `VK_EXT_device_fault` to find the last executed pass.
- Check descriptor indexing: `NonUniformResourceIndex`, partially-bound flags, indices past the array size.

## Profiling
- CPU: the project's `Profiler` for frame spans; look for pipeline creation, descriptor updates, and `vkQueueWaitIdle` in the frame.
- GPU: timestamp queries per pass (`vkCmdWriteTimestamp2`, divide by `timestampPeriod`), and vendor tools (Nsight Graphics GPU Trace, Radeon GPU Profiler, Intel GPA).
- Questions to answer in order: Is the frame CPU- or GPU-bound? Which pass is the longest? Is that pass bandwidth, ALU, occupancy, or latency bound? Does the cost scale with resolution (pixel-bound) or with vertex count?
- Test scale: halve the resolution; if the frame time halves it is pixel-bound; if it does not, look at vertex, CPU or sync costs.
