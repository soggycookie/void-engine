# CLAUDE.md

## Overview
void-engine is a C++20 game engine (namespace `VoidEngine`). It targets Windows first, with a Vulkan backend (VMA, Slang shaders) and a legacy D3D11 backend that is still compiled.

**Current state:**
- The Vulkan calls in `Application::startup` are commented out.
- `renderer.*` and the `GameLayer` draw call are mostly commented out.
- `void/include/platform/vulkan/vulkan_junk.h` is a dead, commented-out allocator experiment.
- `vulkan_renderer_api.*` are empty.

## Build and run
- Configure: `./configure.sh` (`cmake -S . -B build -G Ninja`, clang, Debug). The clang path is hard-coded to a scoop install, so edit it for your machine.
- Build: `./build.sh` (`cmake --build build --parallel`).
- Output goes to `bin/<Config>/`. A post-build step copies `asset/` and `resource/` next to the binary.
- Needs the `VULKAN_SDK` environment variable (Vulkan and Slang are found there). CMake 3.23+.
- Shaders: the `all_shaders` target runs `slangc` on `resource/base/shader/slang/*.slang` and writes SPIR-V to `resource/base/shader/spirv/`.
- There are no tests. `void/test/*.h` is stale and commented out.

## Architecture
```
win32_main.cpp --> Application (singleton)
                    |-- Vallocator (TLSF + Pool, 64 GiB virtual reserve)
                    |-- Window (abstract) <-- Win32Window (platform/win32)
                    |-- LayerStack --> GameLayer (events flow down until handled)
                    |-- JobSystem (<=16 threads, FixedQueue<Job,2048>, JobCounter, 5 priorities)
                    |-- FS:: file system (Win32 async IO, polled by a CRITICAL job)
                    |-- ResourceSystem (AssetId map, Reader/Writer, registry)
                    `-- [disabled] VulkanBackend (instance -> device -> swapchain -> pipeline
                                    -> double-buffered frames, VMA, sync2)
Support: ecs/, containers (dynamic_array, deque, flat_hash_map, stable_vector, String),
         logger, profiler, utils.h
Legacy:  platform/d3d11 (still compiled)
Shaders: resource/base/shader/slang/*.slang --slangc--> spirv/*.spv
```

## File structure
```
void-engine/
|-- CMakeLists.txt, configure.sh, build.sh, .clang-format, .clangd
|-- Cmake/FindVulkan.cmake        custom Vulkan find module
|-- void/
|   |-- include/                  headers (src/ mirrors this tree)
|   |   |-- application.h, game_layer.h, layer.h, layer_stack.h
|   |   |-- window.h, input_event.h, input_manager.h, input/key_button.h
|   |   |-- vallocator.h, arena.h, virtual_mem.h          memory
|   |   |-- dynamic_array.h, deque.h, fixed_queue.h,
|   |   |   flat_hash_map.h, stable_vector.h, str.h       containers
|   |   |-- job_system.h, profiler.h, utils.h, pch.h, engine_config.h
|   |   |-- file_system.h, resource.h, resource_system.h,
|   |   |   resource_registry.h, resource_type_traits.h
|   |   |-- renderer.h, renderer_api.h
|   |   |-- logger/log.h
|   |   |-- ecs/                  world, entity, query, sparse_set, pipeline, type_info (+ .inl)
|   |   `-- platform/
|   |       |-- win32/            win32_window.h, input/win32_vk_mapper.h
|   |       |-- vulkan/           vulkan_backend.h, vulkan_renderer_api.h, vulkan_junk.h (dead)
|   |       `-- d3d11/            d3d11_renderer_api.h (legacy)
|   |-- src/                      .cpp files; same layout as include/
|   |   |-- platform/win32/       win32_main.cpp, win32_window.cpp,
|   |   |                         win32_file_system.cpp, win32_virtual_mem.cpp
|   |   |-- platform/vulkan/      vulkan_backend.cpp, vma_impl.cpp
|   |   |-- platform/d3d11/       d3d11_renderer_api.cpp
|   |   `-- stb_impl.cpp          STB implementation TU
|   `-- test/                     stale, commented-out snippets
|-- resource/base/                engine content
|   |-- shader/slang/*.slang      shader sources
|   |-- shader/spirv/*.spv        compiled output (+ .meta)
|   `-- textures/                 texture.jpg (+ .meta)
|-- asset/shader/platform/d3d11/  game content (HLSL for the D3D11 backend)
|-- external/                     fetched glm, vma, stb, imgui (gitignored)
|-- build/                        Ninja build tree (gitignored)
`-- bin/<Config>/                 executable plus copied asset/ and resource/ (gitignored)
```

Layout: `void/include` and `void/src` mirror each other. Platform code lives in `platform/<win32|vulkan|d3d11>/` on both sides. `resource/` holds engine content and `asset/` holds game content.

## Startup flow
1. `src/platform/win32/win32_main.cpp`: `main()` in Debug builds (with CRT leak checking), `WinMain` in Release. Both run `new Application`, `startup()`, `update()` if startup succeeded, `shutdown()`, `FLUSH_LOG()`, then `delete`.
2. `Application::startup`:
   - Initialize `Vallocator`.
   - `Window::create`, then `Logger::set_window_handle`.
   - Push `GameLayer`. This happens before `window_->init()`, because events go to layers.
   - Start `Profiler`, `JobSystem`, `FS`, and the `FS::poll_io_completions` job.
   - Start `ResourceSystem` on `<cwd>\resource`.
3. `update()`: `on_init` on the layers, then a loop of `window_->update()`, `on_update(dt)` and `on_post_update` until `running_` is false.
4. Events: `Window` calls `Application::on_event`. It handles `APP_CLOSED`, `APP_ENTER_RESIZE` and `APP_EXIT_RESIZE`, then forwards the event down the layers until one marks it handled.

## Conventions
- **Naming:** types are PascalCase, functions and variables are snake_case, and members have a trailing `_`. Old PascalCase functions (e.g. `VulkanBackend::DrawFrame`, `Profiler::StartUp`) are being migrated to snake_case, so write new code in snake_case.
- **Format:** `.clang-format` (Allman braces, 4-space indent, 105 columns).
- **Resource ownership:** every container and every object that owns resources must have an `init`/`destroy` pair. Do not rely on constructors and destructors to acquire and release resources.
- **Memory:** custom containers and `Vallocator` are passed explicitly (e.g. `String::init(Vallocator&)`). Use `construct<T>(alloc, ...)` and `destruct(alloc, p)`. The STL is still used for threads, atomics and similar.
- **Macros** (`utils.h`): `SCAST`/`RCAST`/`DCAST`, `OFFSET_ADDR`, `BYTE_OFFSET`, `kib`/`mib`/`gib`.
- **Logging and asserts** (`logger/log.h`): `LOG_TRACE`/`LOG_INFO`/`LOG_DEBUG`/`LOG_WARN`/`LOG_ERROR`/`LOG_FATAL(channel, fmt, ...)`, `ENGINE_ASSERT(cond, msg)`, `DEBUG_BREAK()`, `FLUSH_LOG()`.

## Gotchas
- Sources are found with `file(GLOB_RECURSE ... CONFIGURE_DEPENDS)`. If a new file is not picked up, re-run configure.
- glm, VMA, stb and imgui are fetched into `external/`, which is gitignored.
- The `imgui` target is built but not linked to the executable.
