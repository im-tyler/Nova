# Godot GPU Physics: Research-Backed Competitive Plan

Last updated: 2026-03-24

## Executive Summary

The goal is to bring GPU-accelerated cloth, fluid, and destruction physics to Godot that competes with Unreal's Chaos system, while supporting Vulkan, WebGPU, OpenGL, and Metal — no vendor lock-in.

The realistic path is:

1. Use **PhysX 5.6** (BSD-3, full GPU source) as the algorithmic reference — not as a runtime dependency.
2. Write solvers in **portable GPU compute** (Vulkan compute / WebGPU compute / OpenGL compute) or use a cross-platform abstraction.
3. Deliver to Godot via **GDExtension** for broad adoption, with engine module escalation if needed.
4. Build cloth first, fluid second, destruction third.

PhysX is the textbook. The implementation is yours, vendor-agnostic, and Godot-native.

## Current Reality on 2026-03-24

### 1. Godot's Built-In Physics

Godot 4.6 ships with **Jolt Physics** as the default 3D physics engine (replaced Bullet). Jolt provides:

- rigid body dynamics (CPU, high quality)
- basic soft body via XPBD (CPU)
- no cloth node
- no fluid simulation
- no GPU acceleration (creator says it's on the wish list, not near-term)
- no destruction/fracture system

**SoftBody3D** exists but is generic soft body, not cloth. Breaks down with constant collisions.

### 2. Godot Community Solutions

| Solution | What It Does | GPU | Quality Level |
|---|---|---|---|
| Jolt (built-in) | Rigid body, basic soft body | No | Production rigid, basic soft |
| Godot Rapier | Rigid body + fluid (Salva) | SIMD only | Good rigid, basic fluid |
| Silkload | Bone-driven cloth | No | Functional for capes |
| Various verlet addons | Simple cloth | No | Prototype |

None approach Unreal's integration or performance.

### 3. Unreal's Chaos System

Chaos is Unreal's unified physics framework providing:

- GPU-accelerated rigid body, cloth (XPBD), destruction (Blast-like)
- deeply integrated with the engine, editor, and rendering
- vertex painting for cloth constraints
- Voronoi fracture for destruction
- Niagara integration for fluid/particle VFX

Chaos is source-available under Epic's EULA. You can read it with an Epic account, but you **cannot** derive from it, copy code, or redistribute for a competing product.

### 4. Open-Source GPU Physics Landscape

#### PhysX 5.6 (NVIDIA, BSD-3)

Released April 2025 with full GPU source code (previously GPU was closed binary).

Covers:

- rigid body dynamics (GPU, 500+ CUDA kernels)
- FEM soft body / deformable (GPU)
- cloth via PxDeformableSurface (GPU, XPBD FEM — but less feature-complete than old NvCloth; no tearing, no raycasts)
- particle-based fluid / granular materials (GPU, PBD)
- Flow SDK: gaseous fluid, smoke, fire (GPU compute shaders, also open-sourced)
- Blast SDK: destruction and fracture (BSD-3, physics/graphics agnostic)

Limitation: **GPU path requires CUDA (NVIDIA-only).** CPU fallback exists but defeats the purpose. Porting 500+ CUDA kernels to Vulkan is theoretically possible but enormous work. No one has done it.

Value: **algorithmic reference.** Every solver algorithm, every data structure, every optimization trick is readable under BSD-3. This is decades of physics engineering, open for study.

#### Newton 1.0 (NVIDIA + Google DeepMind + Disney Research, Linux Foundation)

Released March 2026. GPU-accelerated unified physics:

- MuJoCo-Warp for rigid body
- VBD solver for cloth and deformable
- MPM solver for granular materials
- Built on NVIDIA Warp (Python + CUDA)

Limitation: **CUDA-based via Warp.** Robotics-focused, not game-focused. Python-first API.

Value: **VBD and MPM solver research.** Good reference for alternative solver approaches.

#### Rapier (Dimforge, Apache-2.0)

Rust-based 2D/3D physics engine. 2026 goals include:

- GPU rigid-body physics via rust-gpu
- improved accuracy for robotics

No cloth. Fluid only via separate Salva library (CPU SPH). rust-gpu GPU work is future/experimental.

Value: **Rust + Godot integration reference.** Rapier already ships as a Godot GDExtension. Proves the Rust → GDExtension path works for physics.

#### Bevy Ecosystem (Avian, bevy_xpbd)

Rust game engine using wgpu. Has XPBD physics (CPU). No GPU cloth or fluid yet. Proves wgpu works for game engine compute.

### 5. Cross-Platform GPU Compute Options

The core question: how do you write GPU compute that runs everywhere?

#### Option A: Godot's RenderingDevice (Vulkan compute)

- Write GLSL compute shaders, compile to SPIR-V
- Submit via Godot's RenderingDevice API
- Only works with Godot's Vulkan/D3D12 backends (Forward+, Mobile)
- **Does NOT work** with Compatibility backend (OpenGL/WebGL)
- No WebGPU support yet (open proposal)
- Deepest Godot integration, simplest path

#### Option B: wgpu (Rust)

- Cross-platform GPU abstraction: Vulkan, Metal, D3D12, OpenGL, WebGPU
- Used by Firefox for WebGPU, used by Bevy game engine
- Naga shader compiler: WGSL/SPIR-V/GLSL cross-compilation built in
- Rust-native, pairs with gdext for Godot GDExtension
- Handles platform backends automatically
- Actively maintained, production-quality

#### Option C: Dawn (C++)

- Google's WebGPU implementation (powers Chrome)
- C++ API, maps to Vulkan/Metal/D3D12
- Less Godot ecosystem alignment than wgpu
- Would work for C++ GDExtension or engine module

#### Option D: Write shaders once, cross-compile manually

- Write GLSL → glslang → SPIR-V
- SPIRV-Cross → WGSL (WebGPU), GLSL (OpenGL), HLSL (D3D12), MSL (Metal)
- Or use Slang (compiles to all targets)
- More control, more manual work per platform

#### Option E: Diligent Engine

- C++ cross-platform graphics abstraction
- Supports D3D12, D3D11, OpenGL, Vulkan, Metal, WebGPU
- Less community momentum than wgpu

### 6. WebGL Status

WebGL 2.0 has **no compute shaders.** Never will. WebGPU is the replacement and is now in all major browsers (Chrome, Firefox 141+, Safari 26+, Edge). WebGL is a dead end for GPU physics.

## Analysis

### What Actually Matters

The physics simulation problem breaks into three layers:

**Layer 1: Solver algorithms**
- XPBD for cloth
- SPH/FLIP for fluid
- Voronoi fracture + rigid debris for destruction

These are math. They don't care what GPU API runs them. PhysX 5.6 is the best open-source reference for all of them.

**Layer 2: GPU compute dispatch**
- Buffer management, dispatch, synchronization
- This is where platform portability lives
- Options: Godot RenderingDevice, wgpu, Dawn, manual cross-compile

**Layer 3: Engine integration**
- Godot nodes, editor tools, rendering bridges
- This is where the user-facing product lives
- GDExtension or engine module

### Platform Priority

Based on where Godot actually runs and where users need GPU physics:

| Platform | Priority | GPU Compute Path |
|---|---|---|
| Desktop Vulkan (Linux/Windows) | **Highest** | Vulkan compute |
| Desktop Metal (macOS) | **High** | Metal compute |
| Desktop D3D12 (Windows) | Medium | D3D12 compute |
| Web (modern browsers) | Medium | WebGPU compute |
| Desktop OpenGL 4.3+ | Low | OpenGL compute (wgpu fallback) |
| Mobile (Vulkan) | Future | Vulkan compute subset |
| WebGL | **Skip** | No compute shaders |

### Language Decision

| Approach | Language | GPU Abstraction | Godot Binding | Platform Reach |
|---|---|---|---|---|
| Godot RenderingDevice | C++ | Vulkan only | Native | Vulkan + D3D12 (Godot backends) |
| wgpu + gdext | Rust | Vulkan, Metal, D3D12, OpenGL, WebGPU | GDExtension (Rust) | All major platforms |
| Dawn + godot-cpp | C++ | Vulkan, Metal, D3D12 | GDExtension (C++) | Desktop + Web |
| Engine module | C++ | Godot RenderingDevice | Native | Whatever Godot supports |

## Strategic Options

### Option 1: Godot RenderingDevice Only

Write GLSL compute shaders, dispatch via RenderingDevice from a C++ GDExtension or engine module.

Pros:

- simplest path
- deepest Godot integration
- no external dependencies

Cons:

- Vulkan only (+ D3D12 when Godot adds it)
- no Metal, no WebGPU, no OpenGL compute
- tied to Godot's backend evolution
- if Godot adds WebGPU backend later, you get it for free — but you're waiting

Best if: you only care about desktop Vulkan for now and trust Godot to add more backends.

### Option 2: wgpu (Rust) + GDExtension

Write solvers in Rust, use wgpu for GPU compute, expose to Godot via gdext.

Pros:

- Vulkan, Metal, D3D12, OpenGL, WebGPU from day one
- no vendor lock-in
- Rust safety guarantees for GPU buffer management
- proven path (Rapier already ships as Rust GDExtension)
- wgpu handles shader cross-compilation via Naga
- compiles to WASM for web deployment

Cons:

- two GPU contexts (wgpu and Godot's RenderingDevice) — need to share data via CPU or use interop
- gdext is usable but not fully mature (experimental WASM/mobile)
- Rust learning curve if the team isn't fluent
- wgpu compute results need to be fed back to Godot's renderer (buffer copy overhead)

Best if: cross-platform reach is a hard requirement and Rust is acceptable.

### Option 3: Hybrid — RenderingDevice primary, wgpu for web/Metal

Write solvers as GLSL compute shaders. On Godot's Vulkan/D3D12 path, dispatch via RenderingDevice. For web and Metal, use wgpu as an alternative dispatch backend.

Pros:

- best Godot integration on the primary platform
- cross-platform where needed
- avoids two-GPU-context overhead on the main path

Cons:

- two code paths to maintain
- more complex build system
- shader cross-compilation still needed for the wgpu path

Best if: you want the best of both but accept higher maintenance cost.

### Option 4: Write solvers in portable GLSL, use Slang or manual cross-compilation

Write all solver logic in GLSL compute shaders. Use Slang or SPIRV-Cross to compile to SPIR-V (Vulkan), WGSL (WebGPU), GLSL 4.3 (OpenGL), MSL (Metal). Handle dispatch via a thin C++ abstraction layer.

Pros:

- shader code is the single source of truth
- dispatch layer is thin and replaceable
- works with Godot RenderingDevice for Vulkan, custom dispatch for other backends
- no Rust dependency

Cons:

- you build your own mini GPU abstraction (dispatch, buffers, sync)
- more manual work per platform
- Slang is newer and less battle-tested than wgpu/Naga

Best if: you want C++ throughout and are willing to build the dispatch layer.

## Recommendation

### For maximum platform reach with minimum lock-in:

**Option 2 (wgpu + Rust + GDExtension)** if you're comfortable with Rust.

**Option 4 (portable GLSL + Slang/SPIRV-Cross + C++)** if you prefer C++ throughout.

### For fastest path to a working Godot feature:

**Option 1 (RenderingDevice)** — ship on Vulkan first, expand later.

### The two-GPU-context problem (Option 2)

The biggest technical concern with wgpu is that Godot already has its own Vulkan context via RenderingDevice. Running wgpu alongside it means either:

- CPU-side buffer copies between wgpu and Godot (overhead)
- Vulkan external memory sharing (complex but zero-copy)
- or: wgpu produces vertex/index buffers that Godot imports directly

Rapier (Rust GDExtension) solves this by being CPU-only — it just writes transform data. GPU physics needs to write mesh data, which is harder. This is solvable but needs explicit design.

### The practical middle ground

Start with **Option 1 (RenderingDevice, Vulkan compute)** to prove the solvers and ship the Godot feature. Design the solver code as pure GLSL compute shaders with a clean dispatch interface. When you need more platforms:

- Add WebGPU dispatch when Godot gains a WebGPU backend, OR
- Use SPIRV-Cross/Naga to compile shaders to other targets and add dispatch backends

This avoids the two-GPU-context problem entirely while keeping the door open. The shaders are portable; only the dispatch layer needs porting.

## Solver Reference Sources

### Cloth (XPBD)

- PhysX 5.6 PxDeformableSurface source (BSD-3)
- Newton VBD solver (Linux Foundation)
- Multiple WebGPU XPBD cloth implementations exist as open reference
- Matthias Muller (NVIDIA) XPBD papers and Ten Minute Physics videos
- Bevy XPBD implementation (Rust, CPU, Apache-2.0)

### Fluid (SPH / FLIP)

- PhysX 5.6 particle system source (BSD-3)
- PhysX Flow SDK for gaseous fluids (BSD-3, GPU compute shaders)
- Rapier/Salva for SPH reference (Apache-2.0)
- WebGPU SPH compute examples exist

### Destruction (Fracture)

- NVIDIA Blast SDK (BSD-3, physics/graphics agnostic)
- Voronoi fracture algorithms are well-documented in literature
- Blast designed as a standalone library — easiest to integrate directly

## Recommended Phase Plan

### Phase 0: Feasibility

- prototype XPBD cloth as GLSL compute shaders
- dispatch via Godot RenderingDevice
- validate performance and integration path
- study PhysX 5.6 FEM/XPBD solver architecture
- study Blast SDK architecture for destruction
- benchmark against SoftBody3D and Silkload

### Phase 1: GPU Cloth

- ship ClothBody3D node via GDExtension
- GLSL compute XPBD solver on Vulkan
- skeletal mesh binding, vertex painting, wind
- CPU fallback for compatibility backend

### Phase 2: GPU Fluid

- SPH solver as GLSL compute
- surface reconstruction for rendering
- FluidBody3D node

### Phase 3: Destruction

- integrate or reimplement Blast-style fracture
- FractureBody3D node
- Voronoi pre-fracture, runtime damage, debris

### Phase 4: Platform Expansion

- WebGPU dispatch (when Godot WebGPU backend lands, or standalone)
- Metal dispatch (if needed before Godot supports it)
- OpenGL 4.3 compute fallback

## Sources

### Physics Engines

- PhysX SDK: https://developer.nvidia.com/physx-sdk
- PhysX 5.6 open source announcement: https://www.cgchannel.com/2025/04/nvidia-open-sources-physxs-gpu-simulation-code/
- PhysX GitHub: https://github.com/NVIDIA-Omniverse/PhysX
- PhysX GPU simulation docs: https://nvidia-omniverse.github.io/PhysX/physx/5.4.1/docs/GPURigidBodies.html
- PhysX cloth discussion: https://github.com/NVIDIA-Omniverse/PhysX/discussions/328
- Blast SDK: https://github.com/NVIDIAGameWorks/Blast
- Blast docs: https://nvidia-omniverse.github.io/PhysX/blast/index.html
- Newton physics: https://developer.nvidia.com/newton-physics
- Newton GitHub: https://github.com/newton-physics/newton
- Rapier: https://rapier.rs/
- Rapier 2026 goals: https://dimforge.com/blog/2026/01/09/the-year-2025-in-dimforge/
- Jolt Physics: https://github.com/jrouwe/JoltPhysics
- Jolt GPU discussion: https://github.com/jrouwe/JoltPhysics/discussions/501
- Jolt future: https://github.com/jrouwe/JoltPhysics/discussions/1263

### Cross-Platform GPU Compute

- wgpu: https://github.com/gfx-rs/wgpu
- wgpu guide: https://www.blog.brightcoding.dev/2025/09/30/cross-platform-rust-graphics-with-wgpu-one-api-to-rule-vulkan-metal-d3d12-opengl-webgpu/
- WebGPU browser support: https://www.webgpu.com/news/webgpu-hits-critical-mass-all-major-browsers/
- Diligent Engine: https://github.com/DiligentGraphics/DiligentEngine
- Slang shader language: https://alain.xyz/blog/a-review-of-shader-languages

### Godot Integration

- gdext (Rust GDExtension): https://github.com/godot-rust/gdext
- Godot RenderingDevice: https://docs.godotengine.org/en/stable/classes/class_renderingdevice.html
- Godot compute shaders: https://docs.godotengine.org/en/stable/tutorials/shaders/compute_shaders.html
- Godot WebGPU proposal: https://github.com/godotengine/godot-proposals/discussions/4806
- Godot compatibility compute limitation: https://forum.godotengine.org/t/compatibility-mode-doesnt-support-compute-shaders-nor-dynamic-buffers/110002

### Cloth References

- WebGPU XPBD cloth: https://github.com/jspdown/cloth
- WebGPU cloth simulator: https://github.com/ccincotti3/webgpu_cloth_simulator
- WebGPU cloth research: https://arxiv.org/html/2507.11794v1
- OpenGL compute cloth: https://github.com/likangning93/GPU_cloth
- Bevy XPBD: https://joonaa.dev/blog/02/bevy-xpbd-0-1-0
- Rust game physics engines: https://rodneylab.com/rust-game-physics-engines/

### Unreal Chaos (reference only, EULA-restricted)

- Chaos overview: https://docs.unrealengine.com/4.27/en-US/InteractiveExperiences/Physics/ChaosPhysics/Overview
- Chaos architecture: https://deepwiki.com/mikeroyal/Unreal-Engine-Guide/2.3-chaos-physics-and-simulation
