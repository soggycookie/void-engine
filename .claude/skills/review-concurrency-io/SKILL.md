---
name: review-concurrency-io
description: Checklist for reviewing or designing void-engine JobSystem, threading, async file IO (FS::), Win32 window/event code, and Application startup/shutdown order. Use when a diff touches job_system.*, file_system.*, win32_*, application.*, resource_system.*, or any code that schedules jobs or does async IO.
---

# Concurrency, IO and lifecycle checklist (void-engine)

Paths relative to repo root. Key files: `void/include/job_system.h`, `void/src/job_system.cpp`, `void/include/fixed_queue.h`,
`void/include/file_system.h`, `void/src/platform/win32/win32_file_system.cpp` (about 590 lines), `void/src/application.cpp`,
`void/src/platform/win32/win32_main.cpp`, `win32_window.cpp`.

## JobSystem facts
- `Job { fn_(void *context, void *ex_data); context_; ex_data_; counter_ }`. Plain function pointer, no ownership of `context_`/`ex_data_`.
- `JobCounter { atomic<int32_t> dependency_count; on_zero_priority; on_zero_job }`: when the count reaches 0 the `on_zero_job` is scheduled. The counter's memory is owned by the caller.
- One shared `FixedQueue<Job, 2048>` (`job_pool`) for 5 priorities (`CRITICAL..IDLE`): check how priority maps to queues (a single pool would mean priority is not honored).
- Up to `MAX_WORKER_THREAD = 16` workers, `wake_cv` + `wake_mtx`, `running` is a plain `bool`.
- Singleton via `static JobSystem *Instance` set in the constructor (`ENGINE_ASSERT(!Instance)`). Check that `shutdown`/destructor resets it, or a second startup in the same process aborts.
- `FS::poll_io_completions(void*, void*)` is scheduled as a CRITICAL job from `Application::startup`. It must not block forever or starve workers.

## Check every change against this list
1. **Queue full.** What does `schedule()` do when 2048 jobs are queued (drop, assert, block, spin)? Producers inside jobs can deadlock if they block while all workers are occupied.
2. **Shutdown.** `running` plain bool read by workers = data race unless guarded by `wake_mtx`; workers must be woken (`notify_all`) after the flag is set and joined before the allocator and `FixedQueue` are destroyed. Jobs left in the queue at shutdown: dropped, run, or leaked?
3. **Lost wakeups.** The predicate checked under `wake_mtx` must include queue-nonempty and `!running`. `notify_one` outside the lock is fine only if the state change happened under the lock or is atomic.
4. **Counter lifetime.** `JobCounter` on the stack or freed before every job that references it has finished = use-after-free. `on_zero_job` fires exactly once. The decrement uses `acq_rel` (or `seq_cst`). A counter initialized after jobs were scheduled races.
5. **Captured context.** `context_` pointing to stack data, a `DynamicArray` element, or an object that is destroyed before the job runs.
6. **Memory ordering and sharing.** Anything the job reads must be published before `schedule` (the mutex gives this). Shared containers touched from jobs (`FlatHashMap`, `DynamicArray`, `Vallocator`, logger) need external synchronization. Check which allocator calls from jobs are thread-safe (TLFS/Pool/Arena are probably NOT).
7. **Async file IO (Win32).** `OVERLAPPED` and the read buffer must outlive the operation (heap, not stack). `FILE_FLAG_OVERLAPPED` handle, `GetOverlappedResult` / completion port semantics, `ERROR_IO_PENDING` vs real errors, short reads, offsets above 4 GiB (`OffsetHigh`), `close()` while a request is pending (cancel with `CancelIoEx` first), `MAX_OPENED_FILE = 1024` handle table exhaustion and handle reuse after close (stale `File` handle = ABA), callback invoked on which thread.
8. **Strings and paths.** UTF-8 `StringView` to wide conversion (size, null terminator, failure), `MAX_LOCAL_PATH = 512` truncation, `\\?\` long paths, `/` vs `\`, `get_file_ext` on names without dot or with dot in directory.
9. **GetLastError.** Read it immediately after the failing call, before any logging or allocation that may overwrite it.

## Application lifecycle (from CLAUDE.md, verify against current code)
Startup: Vallocator -> Window::create -> Logger window handle -> push GameLayer (before `window_->init()` because events go to layers) -> Profiler -> JobSystem -> FS -> poll job -> ResourceSystem.
Shutdown must be the reverse and must stop the poll job before FS shuts down, and JobSystem before the allocator it uses.
- `Window` calls `Application::on_event` possibly before startup finishes or during shutdown: check null/uninitialized `layer_stack_`, `running_`.
- Partial-startup failure (`startup()` returns false): `win32_main` still calls `shutdown()`. Every subsystem `shutdown` must tolerate "never started".
- `win32_main` Debug (`main`, CRT leak check) vs Release (`WinMain`) must behave the same. `delete app` after `shutdown`, and static destruction order for singletons.
- Resize/minimize: `APP_ENTER_RESIZE`/`APP_EXIT_RESIZE` toggle `resizing_`. Zero-size window events must not reach the renderer. Modal resize loop on Win32 blocks the message pump, so check that rendering/update still runs or is intentionally paused.
- Delta time: first frame `dt`, huge `dt` after a debugger pause or a drag of the window.

## Reporting
Name the interleaving: "Thread A does X while thread B does Y, then Z". If you cannot write one, mark the finding "Possible".
