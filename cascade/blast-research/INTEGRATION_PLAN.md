# NVIDIA Blast SDK -- Integration Plan for Cascade (Godot/Jolt Physics)

## Status

Research phase. The Blast repo has NOT yet been cloned locally (Bash access was
denied during this session). Clone it manually before proceeding:

```bash
cd /Users/tyler/Documents/physics-sim/blast-research
git clone --depth 1 https://github.com/NVIDIAGameWorks/Blast
```

---

## 1. What Blast Provides

Blast is NVIDIA's replacement for APEX Destruction. It is a **physics-agnostic
and graphics-agnostic** C/C++ library that handles:

| Capability | Description |
|---|---|
| **Pre-fracture generation** | Voronoi, slicing (3-axis subdivision), planar cuts, and bitmap cutout fracture of meshes at edit/author time |
| **Support graph** | Connectivity graph of chunks linked by bonds with health values; determines when pieces separate |
| **Damage model** | User-defined "shader" functions that map damage events (point radial, capsule, shear, triangle intersection, impact spread) to bond/chunk health reduction |
| **Runtime splitting** | After damage depletes bond health, island detection separates the actor into child actors |
| **Stress solver** | Optional gravitational/force-based stress calculation on bonds without needing an external physics engine |
| **Collision geometry** | Convex decomposition of fracture chunks (bundles VHACD) for feeding into any physics engine |
| **Serialization** | Cap'n Proto based cross-platform serialization of assets, families, and actors |
| **Hierarchical destruction** | Multi-depth chunk trees -- a wall can break into large sections, which break into bricks, which break into rubble |

What Blast explicitly does **NOT** include:
- No physics simulation (no rigid bodies, no collision detection)
- No graphics/rendering
- No memory allocators (caller provides memory)

This is exactly what we want: Blast handles *what breaks and when*, while Jolt
handles *where the pieces go physically*.

---

## 2. License

**Nvidia Source Code License (1-Way Commercial)** -- NOT BSD-3 as initially
assumed. Key terms:

- Perpetual, worldwide, non-exclusive, royalty-free copyright license
- May reproduce, prepare derivative works, and distribute
- Derivative works may use different license terms if clearly identified
- Patent retaliation clause (suing NVIDIA terminates your rights)
- No trademark rights
- Provided "AS IS" with no warranty

This is permissive enough for our use. We can vendor the source into our project.

---

## 3. Layered API Architecture

Blast has three layers. We choose which to use:

### Layer 1: NvBlast (Low-Level) -- RECOMMENDED for integration

**Location:** `sdk/lowlevel/`

Pure C-style stateless API. No global state, no framework, no threads, no
memory allocation. The caller allocates all memory and passes it in.

Key source files:
```
sdk/lowlevel/include/
    NvBlast.h              -- All API function declarations (~45 functions)
    NvBlastTypes.h         -- All structs, enums, typedefs
    NvBlastPreprocessor.h  -- DLL export macros
    NvCTypes.h             -- Basic C types
    NvPreprocessor.h       -- Platform detection

sdk/lowlevel/source/
    NvBlastActor.cpp / .h
    NvBlastActorSerializationBlock.cpp / .h
    NvBlastAsset.cpp / .h
    NvBlastAssetHelper.cpp
    NvBlastFamily.cpp / .h
    NvBlastFamilyGraph.cpp / .h
    NvBlastSupportGraph.h
    NvBlastChunkHierarchy.h
```

Core workflow:
1. `NvBlastCreateAsset()` -- build asset from chunk/bond descriptors
2. `NvBlastAssetCreateFamily()` -- create family container
3. `NvBlastFamilyCreateFirstActor()` -- instantiate the undamaged actor
4. `NvBlastActorGenerateFracture()` -- run damage shader to produce fracture commands
5. `NvBlastActorApplyFracture()` -- apply fracture commands, deplete bond health
6. `NvBlastActorSplit()` -- perform island detection, split into child actors

### Layer 2: NvBlastTk (Toolkit) -- OPTIONAL, adds convenience

**Location:** `sdk/toolkit/`

C++ wrapper that adds:
- Global `TkFramework` singleton for object lifecycle
- `TkGroup` for multi-threaded damage processing with worker model
- Event system (`TkEvent` with Split, FractureCommand, FractureEvent, JointUpdate types)
- `TkJoint` for internal joint representation (activates on split)
- `TkFamily` for centralized actor management and event listening

Key classes:
```
TkFramework    -- singleton, creates assets/families/groups/joints
TkAsset        -- wraps NvBlastAsset, adds joint descriptors
TkActor        -- wraps NvBlastActor, adds damage() queue and group processing
TkFamily       -- container for actors from one asset instance
TkGroup        -- parallel processing unit (startProcess/acquireWorker/endProcess)
TkEvent        -- union-style event with Split/Fracture/Joint payloads
TkEventListener -- callback interface for receiving events
```

Source files: 11 headers + 7 implementation files in `sdk/toolkit/source/`.

### Layer 3: Extensions -- USE SELECTIVELY

**Location:** `sdk/extensions/`

| Extension | Purpose | Need for Cascade? |
|---|---|---|
| `authoring` | Voronoi/slice/cutout fracture generation, bond generation, mesh cleaning | **YES** -- edit-time fracture |
| `authoringCommon` | Mesh, Vertex, Triangle types, ConvexMeshBuilder, PatternGenerator | **YES** -- data types for authoring |
| `shaders` | Pre-built damage shader functions (radial, capsule, shear, triangle, impact spread) | **YES** -- runtime damage |
| `stress` | Stress solver for gravitational/force loading on bonds | **YES** -- structural collapse |
| `assetutils` | World bonds, asset merging, geometric transforms | **YES** -- useful utilities |
| `physx` | PhysX-specific actor/family/manager bridge | **NO** -- we write our own Jolt bridge |
| `serialization` | Cap'n Proto serialization | **MAYBE** -- could use for save/load |
| `import` | APEX Destructible Asset conversion | **NO** -- legacy format |
| `exporter` | FBX/OBJ/JSON mesh export | **NO** -- not needed at runtime |

---

## 4. How Fracture Generation Works (Edit Time)

The authoring extension handles pre-fracture. This runs at asset authoring time,
NOT at runtime.

### Input
A triangle mesh (positions, normals, UVs) fed through:
```cpp
Mesh* mesh = NvBlastExtAuthoringCreateMesh(vertices, normals, uvs, vertCount, indices, indexCount);
// or
Mesh* mesh = NvBlastExtAuthoringCreateMeshFromFacets(vertices, edges, facets, ...);
```

### Fracture Tool
```cpp
FractureTool* tool = NvBlastExtAuthoringCreateFractureTool();
tool->setSourceMesh(mesh);  // chunk 0 = original mesh
```

### Fracture Types

**Voronoi Fracture** -- the primary method:
```cpp
VoronoiSitesGenerator* gen = NvBlastExtAuthoringCreateVoronoiSitesGenerator(mesh);
gen->uniformlyGenerateSitesInMesh(siteCount);
// or gen->clusteredSitesGeneration(clusterCount, sitesPerCluster, radius);
// or gen->radialPattern(center, normal, radius, angularSteps, radialSteps, ...);

const NvcVec3* sites = gen->getVoronoiSites();
tool->voronoiFracturing(chunkId, cellCount, sites, replaceChunk);
```

**Slicing** -- axis-aligned subdivision:
```cpp
SlicingConfiguration conf;
conf.x_slices = 3; conf.y_slices = 2; conf.z_slices = 4;
conf.offset_variations = 0.1f;
conf.angle_variations = 0.05f;
tool->slicing(chunkId, conf, replaceChunk);
```

**Plane Cut** -- single planar cut:
```cpp
tool->cut(chunkId, normal, point, noise, replaceChunk);
```

**Cutout** -- 2D pattern projection:
```cpp
CutoutSet* cutout = NvBlastExtAuthoringCreateCutoutSet();
NvBlastExtAuthoringBuildCutoutSet(cutout, bitmap, width, height, ...);
tool->cutout(chunkId, cutout, scale, rotation, offset, ...);
```

### Noise
All fracture methods support `NoiseConfiguration` for adding surface detail:
- amplitude, frequency, octave count, seed

### Output Pipeline
```cpp
// Finalize and generate the NvBlastAsset + render geometry + collision hulls:
AuthoringResult result;
NvBlastExtAuthoringProcessFracture(tool, bondGenerator, collisionBuilder, params, &result);
```

The `AuthoringResult` struct contains:
- `NvBlastAsset* asset` -- the support graph and chunk hierarchy
- `Triangle* geometry` -- render triangles per chunk
- `CollisionHull** collisionHull` -- convex hulls per chunk (from VHACD)
- `NvBlastChunkDesc* chunkDescs` + `NvBlastBondDesc* bondDescs`
- Material name references

### Bond Generation
Bonds (connections between adjacent chunks) are generated automatically:
```cpp
BlastBondGenerator* bondGen = NvBlastExtAuthoringCreateBondGenerator(collisionBuilder);
// Two modes: EXACT (searches for shared surfaces) or AVERAGE (approximate)
```

### Collision Hull Generation
Each chunk gets convex decomposition via bundled VHACD:
```cpp
ConvexDecompositionParams params;
params.maximumNumberOfHulls = 8;     // max convex hulls per chunk
params.voxelGridResolution = 1000000;
params.maximumNumberOfVerticesPerHull = 64;
```

---

## 5. How Runtime Damage/Separation Works

### Data Model
- **Asset**: Immutable. Defines chunk hierarchy, bonds, support graph.
- **Family**: Mutable container. Allocated once per asset instance. Holds all actor state.
- **Actor**: A connected subgraph of the support graph. Starts as one actor (whole object). Splits into multiple actors as bonds break.

### Damage Flow (Low-Level API)

```
1. External event (projectile hit, explosion, etc.)
           |
           v
2. NvBlastActorGenerateFracture(commandBuffers, actor, damageProgram, params)
   - damageProgram contains two function pointers:
     - graphShaderFunction: for multi-chunk actors (damages bonds)
     - subgraphShaderFunction: for single-chunk actors (damages chunks)
   - Damage shaders examine actor geometry and produce fracture commands
           |
           v
3. NvBlastActorApplyFracture(eventBuffers, actor, commands)
   - Reduces bond health values
   - Reduces chunk health values
   - Fills eventBuffers with what actually happened
           |
           v
4. NvBlastActorIsSplitRequired(actor) -> check if bonds are fully broken
           |
           v
5. NvBlastActorSplit(result, actor, maxNewActors, scratch)
   - Performs island detection on the support graph
   - If the graph is still connected: no split, original actor survives
   - If disconnected: original actor is destroyed, N child actors are created
   - Each child actor owns a connected subset of the support graph
           |
           v
6. For each new child actor:
   - Get visible chunk indices -> create physics body + collision shapes
   - Get graph node indices -> track which part of the structure this piece owns
```

### Pre-Built Damage Shaders (from ExtShaders)

| Shader | Description | Params Struct |
|---|---|---|
| Radial Falloff | Damage decreases with distance from point | `NvBlastExtRadialDamageDesc` (position, min/maxRadius, damage) |
| Radial Cutter | Full damage within radius, zero outside | Same struct |
| Capsule Falloff | Damage along a line segment (swept sphere) | `NvBlastExtCapsuleRadialDamageDesc` (posA, posB, radii, damage) |
| Shear | Directional damage with normal vector | `NvBlastExtShearDamageDesc` (position, normal, radii, damage) |
| Triangle Intersection | Damage where a triangle intersects bonds | `NvBlastExtTriangleIntersectionDamageDesc` (3 vertices, damage) |
| Impact Spread | Radial with spreading pattern | `NvBlastExtImpactSpreadDamageDesc` (position, radii, damage) |

Each shader type provides both a `GraphShader` (multi-chunk) and `SubgraphShader` (single-chunk) variant.

### Stress Solver (from ExtStress)

For structural collapse under gravity/load:
```
stress = (bond.linearStress * linearFactor + bond.angularStress * angularFactor) / hardness
```

Workflow:
1. Create solver: `ExtStressSolver::create(family, settings)`
2. Set node info (mass, volume, position) for each graph node
3. Each frame: apply gravity forces via `addGravityForce(actor, localGravity)`
4. Call `update()` to solve stress
5. Query `getOverstressedBondCount()` and generate fracture commands for overstressed bonds

This allows buildings to collapse under their own weight when supports are
destroyed, without needing the physics engine to detect it.

---

## 6. Bridging Debris Pieces to Jolt Rigid Bodies

The PhysX extension (`sdk/extensions/physx/`) provides a reference
implementation of the physics bridge. We replicate this pattern for Jolt.

### PhysX Bridge Architecture (reference)

```
ExtPxManager          -- Central manager, creates families, tracks actors
  ExtPxFamily         -- Per-instance container, spawns/despawns physics actors
    ExtPxActor        -- 1:1 wrapper: TkActor <-> PxRigidDynamic
      ExtPxAsset      -- TkAsset + collision geometry (convex hulls per chunk)
```

Key patterns to replicate:

**1. Asset Registration**
- `ExtPxAsset` wraps `TkAsset` and adds per-chunk collision data
- Each chunk has N sub-chunks, each sub-chunk has a `ConvexMeshGeometry` + local `Transform`
- For Jolt: store `JPH::ConvexHullShape` per sub-chunk instead

**2. Family Spawn**
- `ExtPxFamily::spawn(scene, transform, scale)` creates the initial physics body
- For Jolt: create a `JPH::Body` with a compound shape containing all chunk convex hulls

**3. Split Event Handling**
- When Blast splits an actor, the event listener:
  - Removes the old physics body
  - For each new Blast actor, gets visible chunk indices
  - Creates a new `JPH::Body` with compound shape from those chunks' convex hulls
  - Applies the parent body's linear/angular velocity to children
  - Adds new bodies to the Jolt `PhysicsSystem`

**4. Damage from Physics**
- On Jolt contact callbacks, extract:
  - Contact point position
  - Impact normal
  - Impulse magnitude
- Map the impacted body back to its Blast actor
- Call the appropriate damage shader (radial falloff for impacts, shear for
  sliding contacts)

### Our Jolt Bridge Design

```
BlastJoltManager
  |
  +-- BlastJoltAsset
  |     TkAsset* blastAsset
  |     per-chunk: vector<JPH::ConvexHullShapeSettings>
  |
  +-- BlastJoltFamily
  |     TkFamily* blastFamily
  |     map<TkActor*, JPH::BodyID> actorToBody
  |     JPH::PhysicsSystem* physicsSystem
  |
  |     spawn(transform) -> creates initial body
  |     onSplitEvent(event) -> removes old body, creates children
  |     despawn() -> removes all bodies
  |
  +-- Event handling via TkEventListener:
        onSplit -> create new Jolt bodies from child actors
        onJointUpdate -> create/remove Jolt constraints
```

### Chunk-to-Shape Mapping

For each Blast chunk visible in an actor:
1. Look up the chunk's `CollisionHull` array (from authoring result)
2. For each hull: create `JPH::ConvexHullShapeSettings` from hull vertices
3. Combine into `JPH::StaticCompoundShapeSettings` for multi-hull chunks
4. Combine all visible chunk shapes into actor-level `JPH::MutableCompoundShape`
5. Create `JPH::Body` with this shape, set as dynamic

When a split occurs:
1. Read velocity from the dying body
2. For each child actor's visible chunks, build a new compound shape
3. Create new `JPH::Body` at the correct position
4. Apply parent velocity + any angular component from offset
5. Activate in Jolt's broadphase

---

## 7. Build Requirements

### Dependencies

Blast has minimal external dependencies:
- **C++11 compiler** (MSVC 2017+, GCC, Clang)
- **CMake 3.3+** for the official build system
- No required runtime dependencies (PhysX is optional, only for ExtPhysX)
- Bundled VHACD for convex decomposition (in `sdk/extensions/authoring/source/VHACD/`)

### Official Build

Windows: `generate_projects_vc15win64.bat` (downloads deps via packman)
Linux: `generate_projects_linux.sh`

### For Our Integration (vendored source)

We do NOT use their build system. We vendor the needed source files into our
CMake/SCons build alongside Jolt and Godot. The low-level and extensions are
plain C++ with no special build requirements.

Compile units needed:

```
# Low-level (REQUIRED)
sdk/lowlevel/source/*.cpp                          (6 files)
sdk/common/*.cpp                                    (4 files)

# Toolkit (RECOMMENDED)
sdk/toolkit/source/*.cpp                            (7 files)

# Extensions - Authoring (edit-time fracture)
sdk/extensions/authoring/source/*.cpp               (13 files)
sdk/extensions/authoring/source/VHACD/src/*.cpp     (VHACD library)
sdk/extensions/authoringCommon/source/*.cpp          (unknown count, check after clone)

# Extensions - Damage shaders (runtime)
sdk/extensions/shaders/source/*.cpp                 (3 files: shaders + accelerators)

# Extensions - Stress solver (runtime, optional)
sdk/extensions/stress/source/*.cpp                  (1 file)

# Extensions - Asset utilities
sdk/extensions/assetutils/source/*.cpp              (check after clone)
```

Include paths:
```
sdk/lowlevel/include/
sdk/toolkit/include/
sdk/common/
sdk/extensions/authoring/include/
sdk/extensions/authoringCommon/include/
sdk/extensions/shaders/include/
sdk/extensions/stress/include/
sdk/extensions/assetutils/include/
sdk/globals/include/
```

---

## 8. Estimated Integration Complexity

### Phase 1: Vendor and Build (1-2 days)
- Clone Blast, copy needed source into `cascade/thirdparty/blast/`
- Add to SCons/CMake build
- Resolve any platform-specific compile issues (the code is clean C++ with
  few platform dependencies)
- Expected difficulty: **Low**

### Phase 2: Edit-Time Fracture Pipeline (3-5 days)
- Create Godot editor tool/resource that takes a MeshInstance3D
- Extract triangle data from Godot mesh
- Feed through Blast FractureTool (Voronoi + noise)
- Store AuthoringResult as a custom Godot resource
- Convert collision hulls to Jolt ConvexHullShapes
- Expected difficulty: **Medium** -- main work is Godot mesh <-> Blast mesh conversion

### Phase 3: Runtime Damage + Split (3-5 days)
- Implement BlastJoltManager / BlastJoltFamily / BlastJoltActor
- Wire up Jolt contact listener -> Blast damage pipeline
- Handle split events: remove old body, spawn children with correct velocity
- Handle visual mesh updates (swap visible chunks)
- Expected difficulty: **Medium** -- the PhysX extension is a clear reference

### Phase 4: Stress Solver Integration (1-2 days)
- Wire ExtStressSolver to gravity
- Feed results back through fracture pipeline
- Expected difficulty: **Low** -- self-contained module

### Phase 5: Polish and Optimization (2-3 days)
- Chunk pooling / body recycling for performance
- LOD for distant debris (auto-sleep, merge small pieces)
- Particle/dust effects on fracture events
- Save/load fractured state

**Total estimate: 10-17 days** for a solid integration.

---

## 9. Files to Vendor from Blast into cascade/

After cloning, copy these directories:

```
blast-research/Blast/sdk/lowlevel/           -> cascade/thirdparty/blast/lowlevel/
blast-research/Blast/sdk/toolkit/            -> cascade/thirdparty/blast/toolkit/
blast-research/Blast/sdk/common/             -> cascade/thirdparty/blast/common/
blast-research/Blast/sdk/globals/            -> cascade/thirdparty/blast/globals/
blast-research/Blast/sdk/extensions/authoring/       -> cascade/thirdparty/blast/ext/authoring/
blast-research/Blast/sdk/extensions/authoringCommon/ -> cascade/thirdparty/blast/ext/authoringCommon/
blast-research/Blast/sdk/extensions/shaders/         -> cascade/thirdparty/blast/ext/shaders/
blast-research/Blast/sdk/extensions/stress/          -> cascade/thirdparty/blast/ext/stress/
blast-research/Blast/sdk/extensions/assetutils/      -> cascade/thirdparty/blast/ext/assetutils/
blast-research/Blast/license.txt             -> cascade/thirdparty/blast/LICENSE
```

Do NOT vendor:
- `sdk/extensions/physx/` -- PhysX-specific, we write our own Jolt bridge
- `sdk/extensions/serialization/` -- Cap'n Proto dependency, can add later if needed
- `sdk/extensions/import/` -- APEX legacy format
- `sdk/extensions/exporter/` -- not needed
- `samples/`, `test/`, `tools/`, `docs/` -- not needed in production

---

## 10. Key Architectural Decisions

### Use Low-Level API directly or go through Toolkit?

**Recommendation: Use Toolkit (TkBlast).** The overhead is minimal (7 extra .cpp
files), and it gives us:
- Event system for clean split/fracture notification
- TkGroup for parallel damage processing
- TkJoint for automatic joint management on split
- TkFamily for centralized actor tracking

The low-level API requires manually calling GenerateFracture -> ApplyFracture ->
Split in sequence and manually tracking all state. The toolkit automates this.

### Where does fracture happen?

**Edit time only.** Blast's authoring tools pre-fracture meshes. At runtime we
only do damage -> split -> spawn physics bodies. This is by design and is what
makes Blast fast -- no runtime mesh cutting.

### What about runtime fracture?

If we ever need runtime fracture (e.g., procedural destruction), we could run the
authoring tools at runtime, but this is expensive. The typical pattern is to
pre-fracture at multiple depth levels and let the hierarchy handle progressive
destruction.

### Thread safety?

The low-level API is stateless and thread-safe per-actor. The toolkit's TkGroup
provides a formal parallel processing model. Our Jolt integration should process
damage in the physics thread and dispatch split events to the main thread for
body creation.
