---
name: design-doc-template
description: Template and rules for void-engine design docs in docs/design/<topic>.md, used by the architect agent to resume multi-round design work. Use when starting, updating, or resuming a design doc, or when asked to write down an architecture decision.
---

# Design doc template (docs/design/<topic>.md)

One file per topic, updated in place every round (never a new file per round). Write only under `docs/design/`. Kebab-case file names such as `render-graph.md`.
The first thing a resuming architect does is read this file and the "Status" and "Open questions" sections.

## Template (copy, then fill; delete sections that are empty in an early round)

```markdown
# <Topic>

- **Status:** Draft | In discussion | Decided | Implemented (partially) | Superseded by <file>
- **Round:** <n>   **Updated:** <YYYY-MM-DD>
- **Owner decisions needed:** <count of open questions>

## Goal
<2-3 sentences: what we want and what success looks like. Include non-goals.>

## Context
<What exists today in void-engine that this touches (file paths), what is stubbed or disabled, which CLAUDE.md conventions constrain it: init/destroy pairs, explicit Vallocator, snake_case, Windows first.>

## Assumptions
- A1 ... (mark each: confirmed by user | assumed)

## Design
<Components and responsibilities, data flow, key interfaces (declarations only), memory ownership and allocators, threading model. ASCII diagram if it helps.>

## Decisions
| # | Decision | Chosen | Alternatives rejected (why) | Revisit if |
|---|----------|--------|-----------------------------|------------|
| D1 | | | | |

## Risks and unknowns
| Risk | Impact | Cheapest experiment |
|------|--------|---------------------|

## Open questions
1. <question> (default if unanswered: <X>)

## Implementation plan
1. <step> (project builds and runs after this step) - touches: <files>

## Changelog
- Round n (<date>): <what changed, what the user answered, what decision was reversed and why>
```

## Rules
- Record **why**, not only what. A decision without a reason and a "revisit if" cannot be re-evaluated later.
- Facts about code must carry a file path. Facts about libraries must carry a version and a source (URL) from a search done in this round.
- When an answer reverses an earlier decision, keep the old row, mark it superseded, and note it in the Changelog.
- Open questions move to Decisions as soon as answered; do not delete them silently.
- Implementation steps must each leave the build working (the engine has no tests, so "builds and starts" is the bar).
- Keep it short enough to read in two minutes. Move detailed layouts to a subsection or a sibling file `docs/design/<topic>-<detail>.md` only if the main file passes about 250 lines.
- Do not paste large code. Interface declarations and struct layouts only.
