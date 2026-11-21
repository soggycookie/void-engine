---
name: review-vulkan
description: Checklist for reviewing or designing void-engine Vulkan code (vulkan_backend.*, VMA, swapchain, sync2, frames in flight). Use when a diff touches platform/vulkan/, or when asked to review/audit/fix/design Vulkan init, frame loop, resize, barriers, or teardown.
---

# Vulkan review checklist (void-engine)

Paths are relative to the repo root. Main file: `void/src/platform/vulkan/vulkan_backend.cpp` (about 1800 lines),
header `void/include/platform/vulkan/vulkan_backend.h`. Ignore `vulkan_junk.h` (dead) and `vulkan_renderer_api.*` (empty).
The backend is currently disabled in `Application::startup`, so bugs here are latent until it is re-enabled. Still report them.

## Shape of the code
- Init order: instance -> debug messenger -> surface -> physical device (profile `VP_KHR_ROADMAP_2022`) -> logical device
  -> VMA -> swapchain -> image views -> pipeline -> command pool -> buffers/texture/depth -> descriptors -> command buffers -> sync objects.
- `Context` holds raw handles and `swapchain_images*` / `swapchain_image_views*` raw arrays (not `DynamicArray`).
- `MAX_FRAME_IN_FLIGHT = 2`; per-frame: `SyncObjects{present_complete_sem, render_finished_sem, draw_fence}`, one command buffer, one descriptor set, one mapped UBO.
- Sync2 only: `vkQueueSubmit2`, `VkSemaphoreSubmitInfo`, `VkImageMemoryBarrier2` style helpers (`TransitionImageLayout` x2, `TransitionSwapchainImageLayout`).
- Mixed naming: old PascalCase (`DrawFrame`, `RecordCommandBuffer`, `CreateVertexBuffer`) is being migrated to snake_case.

## Check every change against this list
1. **Creation/destruction symmetry.** Every `vkCreate*`/`vmaCreate*` in startup has a destroy in `shutdown()` in reverse order, and also on each early-return/failure path.
   `shutdown()` must start with `vkDeviceWaitIdle` and destroy VMA buffers/images and `vmaDestroyAllocator` before `vkDestroyDevice`.
2. **VkResult handling.** List each unchecked `vk*` call whose failure is plausible at runtime
   (`vkQueueSubmit2`, `vkResetFences`, `vkAllocateCommandBuffers`, `vkCreate*`, `vmaCreate*`, `vkMapMemory`).
3. **Frame loop order.** Correct pattern: wait `draw_fence[frame]` -> acquire (signal `present_complete_sem[frame]`) -> reset fence only when work will be submitted -> record -> submit (wait acquire sem, signal render sem, fence) -> present (wait render sem) -> `frame = (frame + 1) % N`.
   Check: early `return` after `OUT_OF_DATE` leaves fence/semaphore state consistent; a fence is never reset and then not submitted (deadlock on next wait); `imageIndex` is valid before use.
4. **Semaphore ownership.** `render_finished_sem` is waited on by present, so it is safest indexed per swapchain image, not per frame. An acquire semaphore must not be reused while a prior acquire on it is still pending.
5. **Swapchain recreation.** Handles minimize (extent 0x0: skip recreate and wait for resize), `old_swapchain` passed and destroyed afterwards, image views destroyed first, depth image/view re-created at the new extent, frame-in-flight resources not touched while GPU still uses them (`vkDeviceWaitIdle` or per-frame fence waits), `swapchain_image_count` may change (reallocate the raw arrays), dynamic viewport/scissor or pipeline rebuild if extent is baked in.
6. **Barriers and layouts.** Each image's tracked layout matches `oldLayout`; stage/access masks match the actual usage (color attachment, depth, transfer, sampled). Swapchain image goes `UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC_KHR`. Depth aspect mask includes stencil only if the format has it. Queue family indices are `IGNORED` unless ownership is really transferred.
7. **Dynamic rendering.** `vkCmdBeginRendering` attachment formats match pipeline `VkPipelineRenderingCreateInfo`; load/store ops; clear values.
8. **VMA.** Usage flags vs. memory (`HOST_ACCESS_*` for staging/UBO, persistent map flag), every `vmaCreate*` has a matching `vmaDestroy*`, staging buffers freed after the transfer finishes (not just after submit), `vmaFlushAllocation` for non-coherent memory.
9. **One-shot submits.** `CopyBuffer`, `create_tex_image` use `vkQueueSubmit2` with `VK_NULL_HANDLE` fence: check they wait (`vkQueueWaitIdle` or fence) before freeing staging data or command buffers.
10. **Descriptors/pipeline.** Pool sizes cover `MAX_FRAME_IN_FLIGHT` sets, set layout matches the Slang shader bindings, vertex input matches `Vertex` (offsets/format), shader SPIR-V loaded from `resource/base/shader/spirv/*.spv` with size multiple of 4 and `pCode` 4-byte aligned (a `std::vector<char>` buffer is not guaranteed to be).
11. **Debug-only assumptions.** Code inside `#ifdef VOID_DEBUG` (messenger, validation layer) must not be required for Release correctness. Validation-layer/profile support missing on the user machine must fail gracefully.
12. **Asserts.** `ENGINE_ASSERT(cond, msg)` aborts the process in every configuration (see `logger/log.h`). Do not use it for conditions that can legitimately happen at runtime (`VK_ERROR_OUT_OF_DATE_KHR`, device lost, no suitable GPU). A string literal as `cond` never fires.

## Hazards observed in the code on 2026-11-21 (verify before relying on them; they may be fixed)
- `DrawTest()`: `m_frameIndex = (m_frameIndex++ % s_kMaxFrameInFlight);` leaves the index unchanged (post-increment result is assigned back), so frame 0 resources are always used.
- `DrawFrame()`: `vkWaitForFences` happens after `vkAcquireNextImageKHR` already reused `present_complete_sem[frame]`.
- `DrawFrame()`: `ENGINE_ASSERT("VULKAN", ...)` uses a string literal as the condition, so it can never fail.
- `DrawFrame()`: the non-success/non-suboptimal acquire branch continues with `imageIndex == UINT32_MAX` if the result is `VK_TIMEOUT`/`VK_NOT_READY`.
- `DrawFrame()` ends with `vkQueueWaitIdle`, which serializes CPU and GPU and defeats double buffering. `recreate_swapchain` has no visible zero-extent guard.
- `shutdown()`: no `vkDeviceWaitIdle`, and the texture/uniform/vertex/index destruction is commented out.

## Reporting
For each finding give: function + line, the exact call sequence that goes wrong, and the validation-layer message or crash you would expect.
The project cannot be run headless, so do not claim a runtime repro. Say "by inspection".
