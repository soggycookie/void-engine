---
name: review-ecs
description: Checklist and mental model for reviewing or designing void-engine's archetype ECS (world, entity, query, sparse_set, pipeline, type_info, deferred commands, multithreaded systems). Use when a diff touches void/include/ecs/ or void/src/ecs/, or when asked about entity ids, queries, systems, relations, or deferred command behavior.
---

# ECS review checklist (void-engine)

Files: `void/include/ecs/{world,entity,query,sparse_set,pipeline,type_info,ecs_type,internal_component*}.h` (+ `.inl`),
`void/src/ecs/world.cpp` (about 1600 lines) and `query.cpp`. Namespace `VoidEngine::ECS`. No tests exist, so all review is by inspection.

## Model
- `World` owns its own `Vallocator`, and these sparse sets: `entity_records`, `archetypes`, `cached_queries`, `type_metas`; plus `type_infos` (`FlatHashMap<EntityId, TypeInfo>`), `archetype_map` (`FlatHashMap<TypeSet, Archetype*>`, value holds a reference to the key), `loop_pipeline`.
- Ids: `EntityId` is 64-bit (version + `LowEntityId` low 32). `SparseSet` supports at most 2^32 low ids and pages of 64 entries. Component/tag/relation types are entities too (`TypeId<T>::id()`). `next_free_eid` starts at 200 (ids below are reserved for internal components).
- Structural change = moving an entity between archetypes (`move_archetype_{add,move,copy,remove}_op`), with `swap_back_entity` fixing the record of the entity swapped into the vacated row.
- `EntityCommand`: fixed `add_cmds[16]` and `del_cmds[16]`, counts are `uint8_t`, copied with `memcpy` on move. `EntityBuilder`/patcher batch changes so an entity moves archetype once.
- Deferred mode: `start_defer()` sets a plain bool, `end_defer()` clears it and calls `flush_deferred_cmds()`. Not a nesting counter.
- Multithreading: `set_worker_count`, per-worker `WorkerScratch` (arena + linked list of `EntityCommandNode`; slot 0 = main thread, picked via `thread_local t_scratch_idx`), `EcsBatch` with `Query_T *systems[128]`, atomic `cursor`/`remaining`, run through `JobSystem` (`run_system`, `run_system_main`).
- Cached queries are re-evaluated using `sweep_val` and `revalidate_cached_query_{entity,archetype}_filter` whenever archetypes appear or entities change.

## Check every change against this list
1. **Id reuse and versions.** After `remove_entity`, a stale `EntityId` must fail `is_entity_existing` (version mismatch), not alias a new entity. Check every API taking `EntityId` for validation versus `LowEntityId` (the "skip version" paths such as `is_existing_skip_version`).
2. **Structural change during iteration.** Adding/removing components, destroying entities, or creating archetypes while a query/system iterates over the archetype table (row swap-back moves another entity into the current row, the column arrays may reallocate). It must be deferred, and `deferred` must be true for the entire iteration, including systems that run on workers.
3. **Swap-back bookkeeping.** After a row is removed, the moved entity's `EntityRecord` (archetype row index) is updated, and `revalidate_cached_query_entity_filter` is called with the right row and mode (`ON_ADDED`/`ON_REMOVED`/`ON_MODIFIED`).
4. **Pointers into columns.** `get_mut<T>` / `get(eid, tid)` return pointers that die at the next structural change or column growth. Check for use across `add_component`, `set`, or `create_entity`.
5. **Command limits.** More than 16 adds/removes in one `EntityCommand` = out-of-bounds write. Check that there is a bound check or assert on the `add_count++` path. `uint8_t` count overflow. `sort()` stability and duplicates (add then remove of the same type).
6. **Command ownership.** `AddCommand::data` for `ASSIGN_*` modes: who allocated it, who frees it, moved versus copied (`assign_component_move` leaves the source moved-from). `memcpy`-moved commands copy raw pointers, which is fine only if the source is invalidated (it is, via `add_count = 0`), but check the move-assignment overwrites without leaking a live destination.
7. **Threading.** Two systems writing the same component column in parallel, the main-thread-only API (create/remove entity, archetype creation, `type_infos` mutation) called from a worker, `worker_round_robin` wrap-around, scratch arena reset (`reset_scratch`) while another thread still holds nodes, `t_scratch_idx` initialized for every worker thread, `remaining` reaching 0 only once (batch reuse across frames).
8. **Limits.** `systems[128]` overflow when registering the 129th system, `FlatHashMap` hashing of `TypeSet` (order independence), archetype explosion, `SparseSet::push` with `try_reuse` handing back a dead id with the old version.
9. **Relationships.** `register_relationship`, relation id plus target pair encoding collisions with plain ids, deleting a target entity that others reference (dangling relation), `set_child_of` cycles, `get_parent` on root, parent destroyed before children.
10. **Type registry.** Component registered twice, registered after archetypes using it exist, `TypeMetadata` lifetime vs. archetypes holding `const TypeMetadata*`, non-trivial types: constructor/destructor/move callbacks invoked on archetype moves and on `World::shutdown`.
11. **Shutdown.** `World::shutdown` destroys components with destructors, frees scratches/pipeline, and runs before the JobSystem it used is gone.
12. **Phase pipeline.** `create_bootstrap_phase_dependency` / `create_loop_phase_dependency`, `first_frame` flag, `tick()` ordering; a system added during `tick()`.

## Reporting
Give the exact sequence of world calls (and which thread) that produces the stale pointer, bad row, or lost entity. Prefer a 5-line pseudo-snippet.
