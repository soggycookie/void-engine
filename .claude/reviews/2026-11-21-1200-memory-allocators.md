# Bug inspection: memory allocators (Vallocator, TLFS, Pool, Arena, VirtualMemory)
- Date: 2026-11-21   - Scope: `void/include/vallocator.h`, `void/src/vallocator.cpp`, `void/include/arena.h`, `void/src/arena.cpp`, `void/include/virtual_mem.h`, `void/src/platform/win32/win32_virtual_mem.cpp` (plus callers where needed)   - Reviewed commit/state: 84e1f26 + dirty working tree
- Checklists used: review-memory-containers (manual re-verification of its hazard list)

## Summary
The allocator layer has several defects that trigger in ordinary use. Arena commit growth under-commits (memory overrun), `Arena::in_place_grow` never commits, and `Arena::pop_to` is a no-op, so every scratch scope leaks. TLFS fails for any allocation larger than the committed region, and Debug builds can null-deref on small allocations. Findings: 4 S1, 4 S2, 6 S3, 6 S4. Most important: `Arena::push` commit math (S1), `pop_to` no-op (S1), `in_place_grow` (S1).

## Findings

### [S1-Critical] Arena::push under-commits when request exceeds 2x current commit  (Confirmed)
- **Where:** `void/src/arena.cpp:36-45` (`Arena::push`)
- **Problem:** `next_commit_pos = max(com_size * 2, aligned_extra_size)`, but `aligned_extra_size` is only the *extra* bytes, not an absolute commit position. The required absolute position is `next_pos`. Neither operand is guaranteed to be >= `next_pos` (`2c < next_pos` implies `extra = next_pos - c < next_pos`).
- **Scenario:** Scratch arena (commit 4 KiB, `ThreadScratch::init`), `pos = 0`, push 10 KiB. extra = 6 KiB -> aligned 8 KiB, `com*2` = 8 KiB, so commit grows to 8 KiB. The caller gets a 10 KiB block with 2 KiB uncommitted -> access violation on write. Any file read or path buffer over ~8 KiB in a scratch arena hits this.
- **Suggested fix:** `next_commit_pos = max(com_size * 2, align_up(next_pos, page_size))`, clamped to `res_size`; check the `commit()` result.

### [S1-Critical] Arena::pop_to is a no-op (parameter shadows member)  (Confirmed)
- **Where:** `void/src/arena.cpp:75-81`
- **Problem:** The parameter `pos` shadows the member. `ENGINE_ASSERT(pos <= pos)` is trivially true and `pos = pos` assigns the parameter to itself. `ScopedArenaTemp::end_scratch` (61 call sites in vulkan_backend/win32_file_system/resource_system) and `world.cpp:42` (`pop_to(0)`) never rewind.
- **Scenario:** Every `start_scratch/end_scratch` pair leaks into the 1 MiB-reserve thread scratch arenas. After enough file or resource operations the push hits "Arena capacity exceeded" and aborts. Stale data is never reclaimed.
- **Suggested fix:** Rename the parameter (`new_pos`), assert `new_pos <= this->pos`, then `this->pos = new_pos`.

### [3] [S1-Critical] Arena::in_place_grow extends pos without committing and checks the wrong limit  (Confirmed)
- **Where:** `void/src/arena.cpp:62-72`
- **Problem:** The condition `(res_size - com_size) >= extra_alloc` compares against uncommitted reserve, not `res_size - pos` or committed capacity. The in-place branch does `pos += extra_alloc` without calling the commit path.
- **Scenario:** `DynamicArray`/`String` (`dynamic_array.h:324`, `str.cpp:116`) grows the tail allocation of a scratch arena from 4 KiB to 16 KiB while `com_size` = 4 KiB. `pos` moves past the committed region, then writes fault. Also, when `data_end_pos == pos` but the reserve is nearly exhausted, `pos` can exceed `res_size`.
- **Suggested fix:** Compute `new_end = data_end_pos + extra`. Assert `new_end <= res_size`. Run the same commit-growth logic as `push` before advancing `pos`.

### [4] [S1-Critical] Arena::push/commit growth can overshoot reserve; commit result paths trust asserts only  (Possible)
- **Where:** `void/src/arena.cpp:41-42`, `void/src/vallocator.cpp:83-86`
- **Problem:** `com_size * 2` is not clamped to `res_size`. With a non-power-of-2 commit (reachable after the S1 bug above, or any TLFS `com_size`), `commit` is asked to commit past the reserved range. Windows `VirtualAlloc(MEM_COMMIT)` on free address space reserves and commits arbitrary memory (or fails and aborts).
- **Scenario:** TLFS with commit 3 MiB: doubling reaches 48 GiB, then 96 GiB > 64 GiB reserve.
- **Suggested fix:** Clamp `next_commit_pos` to `reserve_size` / `res_size`.

### [5] [S2-High] TLFS::acquire_memory returns blocks smaller than the request  (Confirmed)
- **Where:** `void/src/vallocator.cpp:78-99`, used at `:560`
- **Problem:** Same flawed formula `max(commit_size*2, aligned_alloc_size)`. The new block is `next_commit_pos - commit_size`, which can be below `size`. It also ignores `sizeof(Header)` and the alignment padding (`padded_size`), because `acquire_memory(size)` is called with `size` not `padded_size`. There is no re-check that the block is big enough. The loop then breaks and `ENGINE_ASSERT(payload >= size)` at `:614` aborts.
- **Scenario:** Vallocator TLFS commit 4 MiB, free space exhausted, `valloc(6 MiB)`: next = max(8 MiB, 6 MiB) = 8 MiB, block = 4 MiB < 6 MiB -> abort. `valloc(4 MiB exactly)` with only the header-sized slack also fails (the header makes the payload too small). An aligned request (`align > 8`) needs `size + align + MIN_BLOCK_SIZE`.
- **Suggested fix:** Compute the needed commit as `align_up(commit_size + padded_size + sizeof(Header), page)`, grow to `max(commit_size*2, needed)` clamped to reserve, and loop `acquire_memory` until `find_next_free_index` succeeds.

### [S2-High] Vallocator/Pool/TLFS used from several threads without synchronisation  (Confirmed)
- **Where:** `void/src/vallocator.cpp:795-822` (Pool), `:911-934` (Vallocator), `void/src/platform/win32/win32_file_system.cpp:350-353` (alloc) vs `:579-586` (free)
- **Problem:** `FS::async_read` allocates the `IO_Request` and buffer from the caller's thread, and `vfree` runs on the `poll_io_completions` job on a worker thread. Pool free lists, `alloc_count`, the TLFS lists and the Debug `debug_map` are all non-atomic.
- **Scenario:** The caller (main thread or another job) calls `valloc(sizeof(IO_Request))` while the poll job calls `vfree(sizeof(IO_Request))`. They race on the same `Pool::head`, so one node is handed out twice or lost (use after free). In Debug, `debug_map` rehash races too.
- **Suggested fix:** Add a mutex (or per-thread caches) to `Vallocator`, or document and enforce single-thread use and give jobs their own allocators.

### [7] [S2-High] Debug TLFS::acquire_block_from_freelist dereferences a null freelist head  (Confirmed, Debug only)
- **Where:** `void/src/vallocator.cpp:216-225` (called from `:565`)
- **Problem:** For `padded_size < 512` the code uses `round_down_to_bin(size)` without checking the bitmap, so the head can be null. The Debug magic check dereferences `block->header` before any null check. In Release the null return is handled.
- **Scenario:** A direct `TLFS::tlfs_alloc(48, 8)` in a Debug build when the exact 48-byte bin is empty -> null deref. Currently reachable via `construct(TLFS&)` / `tlfs_alloc_array` with small sizes. The Vallocator path only sends >4096 bytes here, so it is hidden today.
- **Suggested fix:** `if (!block) return nullptr;` first, or test the bitmap before indexing.

### [S2-High] TLFS size bit-mask clears the upper 32 bits of the header  (Confirmed)
- **Where:** `void/src/vallocator.cpp:118,125` (`& ~3U`), `:383` (`&= ~1U`), `:376` (`&= ~(1U << 1)`), `:659/676` (`&= ~1U`), `:759`
- **Problem:** `~3U`/`~1U` are 32-bit unsigned values, zero-extended when combined with the `uint64_t header.size`. Every mask operation zeroes bits 32-63 of the size.
- **Scenario:** Any block of 4 GiB or more (reachable: reserve is 64 GiB and commit doubles) gets its size truncated on `mark_free`/`mark_used`/`free`. Heap structure corrupts.
- **Suggested fix:** Use `~uint64_t(3)`, `~uint64_t(1)`, `~uint64_t(2)`.

### [9] [RESOLVE] [S3-Medium] round_down_to_bin has no upper bound; FL_COUNT = 24 limits blocks to < 2 GiB  (Confirmed)
- **Where:** `void/src/vallocator.cpp:128-145`, `vallocator.h:69`
- **Problem:** `round_up_to_bin` checks `fli >= FL_COUNT`; `round_down_to_bin`, which `insert_free_block`/`remove_free_block` use, does not. A free block with payload >= 2^31 gives fli >= 24, so `freelists[fli*32+sli]` and `sl_bitmaps[fli]` index out of bounds, writing into `fl_bitmap`, `alloc_count` and neighbours.
- **Scenario:** TLFS commit grows 2 GiB -> 4 GiB -> 8 GiB: the new block payload is 4 GiB-8 -> OOB write. Also reachable by coalescing two large free blocks. The 64 GiB reserve is inconsistent with the 2 GiB bin range.
- **Suggested fix:** Raise `FL_COUNT` to cover the reserve (e.g. 36) and use a 64-bit `fl_bitmap`, or cap the commit step.

### [S3-Medium] Alignment is ignored for pool allocations  (Confirmed)
- **Where:** `void/src/vallocator.cpp:911-921` / `860-884` (`Vallocator::valloc`)
- **Problem:** For `size <= 4096` the `align` argument is dropped. A block is only aligned to its size class (<= 4096 because pages are page-aligned).
- **Scenario:** `valloc(24, 64)` -> class 32 -> 32-byte-aligned block. Normal `sizeof(T)` multiples of `alignof(T)` are fine, but raw calls and `valloc_array` of padded types may violate it. Nothing asserts.
- **Suggested fix:** Pick `size_class = next_pow2(max(size, align))`, or assert `align <= size_class`.

### [S3-Medium] Vallocator::vfree(0 / mismatched size) and release vfree(nullptr) accounting  (Confirmed)
- **Where:** `void/src/vallocator.cpp:923-934`
- **Problem:** The Release `vfree` has no null check, so `vfree(n, nullptr)` decrements `alloc_count` for nothing (the Debug version returns early). A size that crosses the 4096 boundary or a size class between alloc and free sends the pointer to the wrong pool or into TLFS. `Pool::pool_free` only checks range, not alignment or size class, and not double free (Release): a double free creates a cycle and the same block is handed out twice.
- **Suggested fix:** Null-check first, assert `addr` alignment to the class, and compute the free path from the same size rule.

### [12] [S3-Medium] TLFS lost prev-free flag after alignment split; adjacent free blocks stay uncoalesced  (Confirmed)
- **Where:** `void/src/vallocator.cpp:305` (`split`), `:603-621`
- **Problem:** In the aligned path the allocated block's prev-free bit (set by `insert_free_block` of the leading block) is erased by `split(allocated_block, size)` (`header.size = size`). The comment "prev physical block must be used" is false in this path. The same happens in `acquire_memory` (new block does not know the previous block is free; `TODO` at `:96`).
- **Scenario:** After free, leading free block + freed block are adjacent and uncoalesced, so fragmentation. Large allocations then fail despite sufficient total free space.
- **Suggested fix:** Preserve flag bits in `split` (`size | (old & 2)`), and coalesce with the previous last block in `acquire_memory`.

### [S3-Medium] TLFS move constructor is wrong and does not compile in Release  (Confirmed)
- **Where:** `void/include/vallocator.h:94-113`
- **Problem:** Zeroes `alloc_count/alloc_size` on `this` instead of `other`, does not reset `other.fl_bitmap/commit_size/reserve_size/alloc_*`. `std::memcpy(name, other.name, 64)` is outside `#ifdef VOID_DEBUG` and `name` only exists in Debug, so a Release build fails to compile. In Debug `name` is never initialised by the default ctor (`print()` streams garbage if `set_name` was not called).
- **Suggested fix:** Guard the `name` copy, copy counters then zero `other`'s, initialise `name[0] = 0`. Or delete the move ctor (nothing uses it).

### [14] [S3-Medium] TLFS::tlfs_realloc_internal is broken  (Possible, currently unused)
- **Where:** `void/src/vallocator.cpp:632-664`
- **Problem:** (1) The in-place path calls `coalesce(block, next)` on a *used* block; `coalesce` asserts `!is_used_block(prev_block)` -> abort. (2) The fallbacks call `tlfs_alloc_internal` but neither copy `old_size` bytes nor free the old block. (3) `alloc_size` is not updated for the growth. (4) Excess is not split back off. (5) `new_size` is not rounded up to 8 or the min payload.
- **Suggested fix:** Implement realloc as alloc + copy + free, or fix coalesce/accounting before use.

### [S4-Low] Arena::destroy/init lifecycle  (Confirmed)
- **Where:** `void/src/arena.cpp:9-26, 88-94`
- **Problem:** `destroy` does not null `addr` or reset `pos/com_size/res_size`, so a second `destroy` calls `VirtualFree` on an already-released address. If the address range was reused by another allocation, that region is released. `init` does not reset `pos` (stale after destroy+init, and leaks on double `init`). `init` rejects non-power-of-2 sizes via `ENGINE_ASSERT` (e.g. `mib(3)` commit aborts) and never checks `commit_size <= reserve_size`.
- **Suggested fix:** Null/zero in `destroy`, assert not initialised in `init`, drop or relax the pow2 asserts.

### [S4-Low] TLFS::print never prints (inverted loop condition)  (Confirmed, Debug only)
- **Where:** `void/src/vallocator.cpp:27`
- **Problem:** `while (is_header_deadend(header))` should be `!is_header_deadend`. The leak dump in `TLFS::destroy`/corruption diagnostics prints an empty list. `sstream/iostream` includes are assumed via `pch.h`.

### [S4-Low] Overflow in size arithmetic  (Confirmed)
- **Where:** `vallocator.h:321,335,343` (`sizeof(T) * count`), `vallocator.cpp:534` (`align_up(size, 8)`), `:81` (`commit_size + aligned`)
- **Problem:** `count` huge makes `sizeof(T)*count` wrap to a small size, so a small block is returned. `align_up(size, 8)` for `size > UINT64_MAX-7` wraps to 0, then it is bumped to `MIN_PAYLOAD_SIZE`. `padded_size = size + align + MIN_BLOCK_SIZE` is unchecked.
- **Suggested fix:** Checked multiply in `*_array`; reject `size > reserve_size`.

### [S4-Low] VirtualMemory failure paths are swallowed  (Confirmed)
- **Where:** `void/src/platform/win32/win32_virtual_mem.cpp:30-33, 38-46, 55-57, 64-66`
- **Problem:** `reserve_commit` ignores the `commit` return value (a failed commit returns a valid but unusable address; TLFS only asserts `addr`). `decommit` and `release` swallow failures with empty `if` bodies and no logging. `decommit` is never used, so the allocators never return memory. `get_page_size`/`get_system_info` write function-local statics with no synchronisation (data race on a 16-byte struct) and are called from every worker thread via `align_to_commit_size`.
- **Suggested fix:** Use `std::call_once`/magic statics, log `GetLastError`, check commit.

### [S4-Low] Debug double-free detection hole  (Possible)
- **Where:** `void/src/vallocator.cpp:750-768, 696-710`
- **Problem:** When a freed block is coalesced into a neighbour its header (with `MAGIC_NUMBER`) stays inside the merged region. A second `tlfs_free` of that pointer sees `MAGIC` and proceeds, corrupting the free lists. The magic is only set to DEAD on the block that gets inserted. `tlfs_free` also clears the used bit before validating.
- **Suggested fix:** Set `magic = DEAD` on the freed block (and on a merged-away `next`) before coalescing; validate before mutating.

### [S4-Low] Commit-by-doubling over-commits  (Possible)
- **Where:** `void/src/vallocator.cpp:83`, `void/src/arena.cpp:41`
- **Problem:** Each growth commits as much as is already committed. Windows charges commit for this immediately (RAM + pagefile), so a heap that has used e.g. 8 GiB commits another 8 GiB at once and can fail with "Failed to commit" (abort) on a modest machine.

## Edge cases not handled / untested
- `valloc(0, _)`: becomes a 16-byte pool block (works, but `vfree(0)` must be paired); `tlfs_alloc(0)` returns nullptr and `construct` of a 0-size T would assert.
- `Vallocator::valloc` with `size` exactly 4096 (pool, one node per page) and 4097 (TLFS): `vallocator.cpp:863`. Both OK, but untested with `align` > 8.
- `Arena::push(0, ...)` at `pos == res_size`: allowed.
- `Arena::push` with `align > allocation granularity (64 KiB)`: offset-based alignment is not address alignment (`arena.cpp:32`).
- `TLFS::init(res, 0)`: `commit_size = 0` makes the first allocation take the grow path, which then computes `max(0, ...)`.
- TLFS init failure: `ENGINE_ASSERT(addr)` aborts instead of returning an error; `TLFS::destroy` on an uninitialised/already-destroyed allocator aborts via `VirtualMemory::release(null)` (`win32_virtual_mem.cpp:62`) or calls `VirtualFree` on a stale address.
- `Pool::acquire_page` loop index is `uint32_t`; `size_class` > page_size would give `num_node == 0` and an empty list -> `pool_alloc` assert (not reachable with the current 16..4096 classes).
- Allocations > reserve (e.g. `valloc(70 GiB)`): an abort by assert, not a recoverable null.

## Pitfalls & notes
- `Vallocator::destroy` does not assert leaks (the assert is commented out); only TLFS leaks abort, pool leaks are silent (`Pool::alloc_count` never checked).
- Pool/arena memory is never returned to the OS. Pool pages are never reused across size classes.
- `valloc_free_array` and `vfree` must be given the exact allocation size/count (see checklist).
- Debug vs Release differ: `MIN_BLOCK_SIZE` 64/32, `Header` 24/8 bytes, Debug builds route `debug_map` allocations through the same TLFS.
- `ENGINE_ASSERT` (abort in all builds) is used for OOM and capacity conditions in `Arena::push`, `acquire_memory`, `construct`; no caller can recover.
- Conventions: `Header::size` flags use magic numbers (`1U << 1`); `relloc_block`, `blockPayLoadAddr`, `newBlock` are camelCase locals; `vallocator.h` pulls `<source_location>` into release signatures of the array helpers (they pass `loc` to a release `tlfs_alloc` that takes no `loc`, so `tlfs_alloc_array`/`tlfs_free_array`/`valloc_array`/`valloc_free_array` do not compile in Release if instantiated).

## Not reviewed
- `FlatHashMap`, `DynamicArray`, `StableVector`, `String` internals beyond the call sites that touch Arena (separate pass).
- Vulkan/VMA allocation code and `vulkan_junk.h` (dead).
- No build or run was performed.
