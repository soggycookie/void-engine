---
name: review-memory-containers
description: Rules and checklist for void-engine memory and container code (Vallocator, TLFS, Pool, Arena, DynamicArray, FlatHashMap, StableVector, Deque, FixedQueue, String) and the init/destroy ownership convention. Use when a diff touches vallocator.*, arena.h, virtual_mem.h, containers, str.h, or any code that allocates, or when designing a new container or allocator user.
---

# Memory and container checklist (void-engine)

Paths relative to repo root, headers in `void/include/`, sources in `void/src/`.

## Allocator model
- `Application` owns a `Vallocator` (64 GiB virtual reserve). `Vallocator` = `TLFS` (TLSF, general) + `Pool[]` (size classes 16..4096 by powers of two, backed by `page_arena`) + `Arena page_arena`.
- Alloc API: `valloc(size, align)` / `vfree(size, ptr)`. **`vfree` takes the size**: freeing with a different size than allocated sends the block to the wrong pool or the wrong path. `construct<T>(alloc, ...)` / `destruct(alloc, p)` and `valloc_array<T>` / `valloc_free_array<T>(alloc, p, count)` wrap them; the count passed to free must equal the count allocated.
- `TLFS` has two overloads of `construct`/`destruct` (TLFS& and Vallocator&); the TLFS free does not take a size.
- Debug builds (`VOID_DEBUG`) add `source_location` params, a `DebugTag` header (magic `0xA7F3C91D` live / `0xDEADDEAD` dead) and a `debug_map` in `Vallocator`: Debug and Release have different block sizes (`MIN_BLOCK_SIZE` 64 vs 32) and header layout, so a bug can hide in one configuration.
- `Arena` (reserve/commit, `in_place_grow`) is for scratch/lifetime-scoped data. `ThreadScratch` is per-thread. Nothing in an arena is individually freed.
- Project rule: every container and every object owning resources has `init(...)` / `destroy(...)`. Constructors only zero-initialize. Destructors must not free.
- `ENGINE_ASSERT` aborts in all configs, and `construct` asserts the allocation succeeded (so OOM = abort, not a returned null).

## Container facts to check against
- `DynamicArray<T>`: `init(Arena&, cap)`, `push_back/emplace_back/resize` overloaded for both `Arena&` and `Vallocator&`, but `destroy(Vallocator&)` only. Mixing an arena-grown array with a `destroy(vallocator)` is a bug. A reference or pointer to an element is invalidated by any `push_back`/`resize`. `set_count` does not construct elements. `pop_back` on empty.
- `FlatHashMap<K,V>`: `init`/`destroy` for both `TLFS&` and `Vallocator&`. Rehash invalidates all references and iterators. Check the tombstone/erase-during-iteration behavior, load factor, key hash quality (`AssetId`, `EntityId`), and zero or sentinel keys.
- `StableVector<T>`: handle-based, pages never move. Still has `StableHandle` generation checks (uses the `LOG_ASSERT` macro, a log + debug-break loop, not an abort). Check destroy of live elements and handle reuse.
- `Deque`, `FixedQueue<T,N>`: wraparound index math at exactly N, full vs. empty ambiguity, behavior on push when full.
- `String`/`StringView`: `String::init(Vallocator&)`, null terminator, `StringView` outliving the `String` that backs it (very common: a view into a `DirectoryInfo::abs_path` after the `Arena` was reset).
- Container types with raw pointer members need a correct move constructor: moved-from object must be reset so a later `destroy` is a no-op.

## Check every change against this list
1. **Pairing.** Each `init` has a `destroy` on all paths (early return, failed startup, exception-free error paths) and `destroy` is idempotent or clearly guarded. No double destroy, no use after destroy. `init` twice leaks.
2. **Allocator identity.** Memory freed through the same allocator and size it came from (Vallocator vs TLFS vs Arena vs `new`/`malloc`).
3. **Growth invalidation.** Hold-a-pointer-then-push patterns: `T &r = arr[i]; arr.push_back(...); use(r);`. Same for `FlatHashMap` rehash, `SparseSet` pages, `World` archetype columns.
4. **Overflow and alignment.** `sizeof(T) * count` overflow in `valloc_array`, align larger than `DEFAULT_ALIGNMENT = 8` requested from a pool (pools hand out size-class blocks, check that the alignment holds), TLSF split/coalesce sizes, `MIN_PAYLOAD_SIZE` rounding, off-by-one at `MAX_SIZE_CLASS = 4096` (the boundary goes to a pool or TLFS?).
5. **Moves.** Move constructor/assignment resets the source completely (every field, including counters and bitmaps), and self-move is safe.
6. **Trivially-relocatable assumptions.** `memcpy` of types that are not trivially copyable (for example `EntityCommand` memcpy's `AddCommand` with raw `data` pointers).
7. **Arena resets.** Resetting an arena while anything still points into it; `Arena` reused across threads.
8. **Thread safety.** None of `TLFS`/`Pool`/`Arena`/containers is assumed thread-safe. Any use from a job needs a per-thread allocator or a lock.
9. **Leak accounting.** `get_alloc_count()` / `get_alloc_size()` nonzero at `Vallocator::destroy` is a leak. Debug builds can name the site via `debug_map`.

## Hazard observed in the code on 2026-11-21 (verify; may be fixed)
- `TLFS(TLFS&&)` (vallocator.h): after copying fields it sets `alloc_count = 0; alloc_size = 0;` on `this` (the destination) instead of on `other`, and does not reset `other.fl_bitmap` / `other.commit_size` / `other.reserve_size`. The moved-from allocator keeps its counters, and the new one loses them.

## Reporting
State which allocator/size/lifetime is violated and at which line, and say whether it only shows up in Debug or Release.
