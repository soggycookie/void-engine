---
name: implement-design
description: Implement a design doc that the architect agent wrote in docs/design/<topic>.md, one plan step at a time, and merge in the user's own design decisions. Use when asked to "implement the design", "build step N of the plan", "implement what the architect output", "continue the imgui plan", or to apply the user's extra design changes on top of a design doc.
---

# Implement a design doc (architect output + human design)

Paths are relative to the repo root (`void-engine/`). The architect writes `docs/design/<topic>.md`
(format: `.claude/skills/design-doc-template/SKILL.md`). This skill turns its **Implementation plan**
into code. The user's own design instructions in the current conversation are binding and override the doc.

## 1. Load the design and fix the scope
1. Read `docs/design/<topic>.md` fully. No topic given: list `docs/design/` and take the newest. If several are plausible, ask.
2. Note: **Status**, **Open questions** (with defaults), **Decisions** (D#), **Implementation plan** and any "Scope for now" paragraph. Deferred steps are not built.
3. Pick the step: the one the user names, otherwise the first step that is not marked done or deferred. Build **one step per run**. Each step must leave the build working.
4. Load the matching checklist skill for the files the step touches (review-vulkan, review-concurrency-io, review-memory-containers, review-ecs). Use `subsystem-map` when unsure who owns something.

## 2. Merge the human design (extra design)
The user's words in this conversation are an amendment to the doc, not a suggestion.
- Classify each user instruction: **new decision** (changes a D# or adds one), **scope change** (adds, drops or defers a step), **answer** to an open question, or **implementation detail** (naming, file layout, style).
- Where it conflicts with the doc, follow the user. Where the user's request would reintroduce a risk the doc lists, do it anyway and say so in one line.
- If an instruction is a design change larger than the step (new ownership, new public API, different threading), do not improvise: update the doc first (see section 5), then implement.
- Never ask about things the doc already defaults. Use the default, say which one you used.

## 3. Implement
- Work in the dirty tree. Run `git status --short` first. Read each file's current state before editing. Never `git checkout/restore/reset/stash/clean`. Do not commit unless asked.
- Touch only the files listed in the step's "touches". A needed extra file is fine, mention it.
- Follow CLAUDE.md: Allman, 4 spaces, 105 columns, snake_case functions, trailing `_` members, `init`/`destroy` pairs (no resource work in constructors/destructors), explicit `Vallocator&`, `construct<T>`/`destruct`, `LOG_*`, `SCAST/RCAST/DCAST`.
- New `.cpp`/`.h` files are globbed: re-run `./configure.sh` (or `cmake -S . -B build`) after adding one.
- Do not edit `external/`, `build/`, `bin/`, `.claude/reviews/*`, or code CLAUDE.md marks dead.
- For design-doc interfaces ("declarations only"), match the declared names and signatures unless the user changed them.
- Shutdown order and init order in the doc are requirements, not hints.

## 4. Verify
- Build: `./build.sh` from the repo root (it wraps `cmake --build build --parallel`). Under PowerShell run `bash ./build.sh`. Exit code 0 plus "Build completed." means success; the script uses `set -e`, so any compile error gives exit 1. `build/` must already be configured with Ninja and `VULKAN_SDK` set.
- Read the errors for files you touched only. State of the tree at the time this skill was written: the build **already fails** in files not related to the design (`vulkan_backend.cpp:987`: unknown type `VulkanAllocationInfo`, plus a type-mismatch error at a `winuser.h` `hCursor` parameter, 20 errors total, from the uncommitted ECS/Vulkan work). Filter the error list for the files you touched. Never claim "build passes" when it does not; say `not built clean: errors are in <files>, none in mine`.
- If the step says "starts" (no visible change), run `bin/<Config>/` executable only if the build links. Otherwise say it was not run.
- There are no tests. Do not invent any.
- Walk the doc's **Risks** table: does this step trip one? Report it.

## 5. Write back to the design doc
Update `docs/design/<topic>.md` in place (never a new file):
- Mark the plan step done with the date, and note "not compile-verified" if the build was broken.
- Record every user decision as a new D# row (chosen, rejected and why, revisit if). Mark reversed rows superseded, do not delete them.
- Move answered open questions to Decisions. Bump **Round**, **Updated**, **Owner decisions needed**.
- Add a Changelog line: what was built, what the user changed.
- Doc rules: facts about code carry a file path, no large code pasted.

## 6. Reply
Short. Four parts: step built, files changed, user decisions applied (and which doc rows changed), build state plus the next step. For design questions that need a decision, route to the `architect` agent. For bugs found on the way, mention them and leave them (bug-inspector / code-plumber own those).

## Gotchas
- The doc can be stale versus code. Re-verify every cited `file:line` before relying on it.
- Doc status "In discussion" with unanswered questions still has usable defaults; do not block on them unless the step depends on one that has no default.
- `Application::startup` order matters: layers are pushed before `window_->init()`; Vulkan/ImGui order is given in the doc's **Order** bullet.
- `Vallocator::vfree` needs the real size: destroy layers by their real type, not through `Layer*`.
