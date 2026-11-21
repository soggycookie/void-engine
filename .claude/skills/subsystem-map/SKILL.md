---
name: subsystem-map
description: One-page map of void-engine subsystems - who owns what, public entry points, dependencies, and what is built vs stubbed or disabled. Use at the start of any design or review task to avoid re-grepping the code base, and when asked "where does X live" or "what depends on Y".
---

# void-engine subsystem map

Snapshot from 2026-11-21 (git HEAD `84e1f26` plus uncommitted work). Treat statuses as hints and re-verify a file with Read before relying on it.
Paths are relative to repo root; headers in `void/include/`, sources in `void/src/` (same tree). Conventions and build steps live in `CLAUDE.md`.

| Subsystem | Entry points / files | Owner and lifetime | Depends on | Status |
|---|---|---|---|---|
| Application | `application.h/.cpp`, `platform/win32/win32_main.cpp` | Singleton created in `main`/`WinMain`. Owns the members below | everything | Working. Vulkan call sites commented out |
| Memory | `vallocator.h/.cpp` (TLFS + Pool + Arena), `arena.h`, `virtual_mem.h`, `platform/win32/win32_virtual_mem.cpp` | `Application::vallocator_` (64 GiB reserve). `ResourceSystem` and `World` and `VulkanBackend` have their own `Vallocator` | OS virtual memory | Working. See `review-memory-containers` |
| Containers | `dynamic_array.h`, `deque.h`, `fixed_queue.h`, `flat_hash_map.h`, `stable_vector.h`, `str.h`, `container_converter.h` | The caller, through `init`/`destroy` with an explicit allocator | Vallocator/Arena | Working. No tests |
| Logging | `logger/log.h`, `logger/log.cpp`; macros `LOG_*`, `ENGINE_ASSERT` (abort) / `LOG_ASSERT` (break loop) | Global | Window handle for message boxes | Working |
| Window and input | `window.h` (abstract), `platform/win32/win32_window.*`, `input_event.h`, `input_manager.h`, `input/key_button.h`, `platform/win32/input/win32_vk_mapper.h` | `Application::window_` (raw pointer, `Window::create`) | Application::on_event | Working. Resize flow via `resizing_` |
| Layers | `layer.h`, `layer_stack.h`, `game_layer.h` | `Application::layer_stack_` | Events (top to bottom until handled) | `GameLayer` draw mostly commented out |
| Jobs | `job_system.h/.cpp` (<=16 threads, `FixedQueue<Job,2048>`, `JobCounter`, 5 priorities) | `Application::job_system` | Vallocator | Working. See `review-concurrency-io` |
| File system | `file_system.h`, `platform/win32/win32_file_system.cpp`, `asset_file_system.h` (header only, no .cpp) | Namespace `FS::`, global state. `poll_io_completions` runs as CRITICAL job | JobSystem, Win32 async IO | Working. Handle table `MAX_OPENED_FILE=1024` |
| Resources | `resource.h`, `resource_system.h/.cpp`, `resource_registry.h`, `resource_type_traits.h` | `Application::resource_system_` (rooted at `<cwd>\resource`) | FS, Vallocator | Partial. `.meta` files, `AssetId` map, `acquire/release`, bundle file parsing is only sketched in comments |
| ECS | `ecs/*.h`, `.inl`, `src/ecs/world.cpp`, `query.cpp` | `ECS::World` (own Vallocator). Referenced from `game_layer.h` (the only non-ECS user), not owned by Application | Containers, JobSystem (multithread systems) | Large and mostly self-contained. See `review-ecs` |
| Profiler | `profiler.h`, `src/profiler.cpp` | Started in `Application::startup` | - | Working, old PascalCase API (`StartUp`) |
| Renderer facade | `renderer.h/.cpp`, `renderer_api.h` | - | Backend | Mostly commented out |
| Vulkan backend | `platform/vulkan/vulkan_backend.h/.cpp`, `vma_impl.cpp`; shaders `resource/base/shader/slang/*.slang` -> `spirv/*.spv` | `VulkanBackend` (friend of Application), disabled | Window, VMA, Vulkan profiles, Slang | Triangle/quad with texture and depth existed. See `review-vulkan` |
| D3D11 backend | `platform/d3d11/d3d11_renderer_api.*`, `asset/shader/platform/d3d11/` | Legacy | - | Still compiled, unused |
| Dead/stale | `platform/vulkan/vulkan_junk.h` (1500 lines, commented), `vulkan_renderer_api.*` (empty), `void/test/*` | - | - | Ignore unless asked |

## Startup order (Application::startup)
Vallocator -> Window::create -> Logger::set_window_handle -> push GameLayer -> window init -> Profiler -> JobSystem -> FS (+ poll job) -> ResourceSystem. Shutdown is the reverse.

## Where the risk is (largest and most-changed files)
`vulkan_backend.cpp` (1.8k lines), `ecs/world.cpp` (1.6k), `vallocator.cpp` (0.8k), `win32_file_system.cpp` (0.6k). The git status at snapshot time shows uncommitted changes in application, window, file system, resource system, logger, dynamic_array, and the Vulkan backend.

## Keeping this file honest
If you find this map wrong while working, fix the row in place and update the snapshot date. Do not add prose; keep one row per subsystem.
