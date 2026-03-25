# Runtime Delivery Feasibility Memo

Last updated: 2026-03-23

## Question

What is the most credible way to ship the Meridian runtime inside Godot?

## Options

### Option A: pure GDExtension runtime

Pros:

- easiest distribution story
- least invasive to engine source
- good fit for importer and editor tooling

Cons:

- unclear ownership of the opaque rendering path
- unclear shadow-path ownership
- unclear ability to integrate a full dense-geometry renderer cleanly through public hooks alone

Current judgment:

- high risk as the sole runtime plan

### Option B: hybrid extension plus engine patch

Pros:

- importer and tooling can remain extension-driven
- runtime can gain the renderer ownership it needs
- lower friction than a fully separate long-term fork

Cons:

- more moving parts
- still requires engine-side maintenance

Current judgment:

- strongest near-term delivery candidate

### Option C: full engine module

Pros:

- clearest renderer ownership
- strongest performance and integration path
- simplest conceptual model for the runtime renderer itself

Cons:

- heavier maintenance and distribution burden
- larger upstreaming challenge

Current judgment:

- credible if Option B becomes too awkward

## Current Recommendation

Plan as if the runtime will be **Option B: hybrid extension plus engine patch** until benchmark and feasibility work prove otherwise.

Use:

- extension space for importer, resources, benchmark controls, and debug UX
- engine-owned runtime surfaces for the actual dense-geometry renderer if needed

## What Must Be Proven in Phase 0

1. Whether current public renderer hooks can own enough of the runtime path.
2. Whether shadow integration is feasible without engine ownership.
3. Whether material integration for the v1 subset is practical through public APIs alone.

## Exit Condition

Close this memo once a concrete runtime ownership decision is made and reflected in the project structure.
