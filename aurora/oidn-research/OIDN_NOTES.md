# Intel Open Image Denoise (OIDN) -- Integration Research for Project Aurora

Version studied: **2.4.1** (current stable, Apache 2.0 license)
OIDN 3 (temporal denoising) expected **H2 2026**.

---

## 1. API Overview

OIDN exposes a C99 API (`oidn.h`) and a C++11 wrapper (`oidn.hpp`). All objects are reference-counted.

### Device Creation

```c
// Auto-select fastest device (GPU preferred, CPU fallback)
OIDNDevice device = oidnNewDevice(OIDN_DEVICE_TYPE_DEFAULT);
oidnCommitDevice(device);

// Or target a specific backend
OIDNDevice device = oidnNewDevice(OIDN_DEVICE_TYPE_CUDA);

// Match a Vulkan physical device by UUID (critical for buffer sharing)
OIDNDevice device = oidnNewDeviceByUUID(vulkanPhysicalDeviceUUID);
oidnCommitDevice(device);
```

Available device types: `CPU`, `SYCL`, `CUDA`, `HIP`, `METAL`, `DEFAULT`.

Backend-specific constructors exist for passing native handles:
- `oidnNewCUDADevice(deviceIDs, streams, numPairs)`
- `oidnNewHIPDevice(deviceIDs, streams, numPairs)`
- `oidnNewMetalDevice(commandQueues, numQueues)`
- `oidnNewSYCLDevice(queues, numQueues)`

Physical device enumeration: `oidnGetNumPhysicalDevices()` + query UUID/LUID/PCI address per device.

### Filter Creation

Two filter types:
- **"RT"** -- General ray tracing denoiser. Accepts color (required), albedo (optional), normal (optional). Supports LDR and HDR.
- **"RTLightmap"** -- HDR lightmap and directional lightmap (SH) denoiser. HDR only.

```c
OIDNFilter filter = oidnNewFilter(device, "RT");
oidnSetFilterImage(filter, "color",  colorBuf,  OIDN_FORMAT_FLOAT3, width, height, 0, 0, 0);
oidnSetFilterImage(filter, "albedo", albedoBuf, OIDN_FORMAT_FLOAT3, width, height, 0, 0, 0);
oidnSetFilterImage(filter, "normal", normalBuf, OIDN_FORMAT_FLOAT3, width, height, 0, 0, 0);
oidnSetFilterImage(filter, "output", outputBuf, OIDN_FORMAT_FLOAT3, width, height, 0, 0, 0);
oidnSetFilterBool(filter, "hdr", true);
oidnSetFilterInt(filter, "quality", OIDN_QUALITY_BALANCED);
oidnCommitFilter(filter);  // compiles the network -- expensive, do once
oidnExecuteFilterAsync(filter);
oidnSyncDevice(device);
```

Key rule: `commit()` must be called after any parameter change. Reuse the same filter object across frames if dimensions/features stay constant.

### Buffer Management

```c
// OIDN-allocated buffer (device-local)
OIDNBuffer buf = oidnNewBufferWithStorage(device, byteSize, OIDN_STORAGE_DEVICE);

// Wrap user-owned pointer (only if device supports system memory access)
OIDNBuffer buf = oidnNewSharedBuffer(device, ptr, byteSize);

// Import from Vulkan via external memory (see section 2)
OIDNBuffer buf = oidnNewSharedBufferFromFD(device, OIDN_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_FD, fd, byteSize);
```

Storage modes:
| Mode | Description |
|------|-------------|
| `HOST` | Pinned memory, accessible by CPU and device |
| `DEVICE` | GPU-only, best performance |
| `MANAGED` | Auto-migrates (device-dependent support) |

Async read/write for device-storage buffers:
```c
oidnReadBufferAsync(buf, offset, size, hostPtr);
oidnWriteBufferAsync(buf, offset, size, srcPtr);
oidnSyncDevice(device);
```

---

## 2. Vulkan Buffer Sharing (Avoiding CPU Roundtrip)

OIDN does NOT have a native Vulkan backend. Instead, it uses CUDA/HIP/SYCL/Metal internally but can **import Vulkan-exported memory** to avoid host-side copies.

### Workflow

1. **Match physical devices.** Create the OIDN device on the same GPU as Vulkan:
   ```c
   // Query Vulkan: vkGetPhysicalDeviceProperties2 -> VkPhysicalDeviceIDProperties.deviceUUID
   OIDNDevice device = oidnNewDeviceByUUID(vulkanDeviceUUID);
   ```
   Fallback to LUID or PCI address if UUID matching fails (driver inconsistencies exist).

2. **Query supported external memory types:**
   ```c
   int flags = oidnGetDeviceInt(device, "externalMemoryTypes");
   // Check against OIDNExternalMemoryTypeFlag values
   ```

3. **Export Vulkan buffer memory** using `VK_KHR_external_memory` + `VK_KHR_external_memory_fd` (Linux) or `VK_KHR_external_memory_win32` (Windows):
   ```c
   // Vulkan side: allocate with VkExportMemoryAllocateInfo
   // Then: vkGetMemoryFdKHR or vkGetMemoryWin32HandleKHR
   ```

4. **Import into OIDN:**
   ```c
   // Linux
   OIDNBuffer buf = oidnNewSharedBufferFromFD(device,
       OIDN_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_FD, fd, byteSize);

   // Windows
   OIDNBuffer buf = oidnNewSharedBufferFromWin32Handle(device,
       OIDN_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32, handle, NULL, byteSize);
   ```

5. **Use imported buffer as filter image:**
   ```c
   oidnSetFilterImage(filter, "color", buf, OIDN_FORMAT_FLOAT3, width, height, 0, 0, 0);
   ```

6. **Synchronize on host** (no imported semaphore support yet):
   ```c
   // After Vulkan writes are done (vkQueueWaitIdle or fence)
   oidnExecuteFilterAsync(filter);
   oidnSyncDevice(device);
   // Now safe for Vulkan to read the output buffer
   ```

### Supported External Memory Flags

| Flag | Platform | Vulkan Equivalent |
|------|----------|-------------------|
| `OPAQUE_FD` | Linux | `VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT` |
| `DMA_BUF` | Linux | `VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT` |
| `OPAQUE_WIN32` | Windows | `VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT` |
| `OPAQUE_WIN32_KMT` | Windows | `VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT` |
| `D3D11_TEXTURE` | Windows | N/A (Direct3D 11) |
| `D3D12_HEAP` | Windows | N/A (Direct3D 12) |
| `D3D12_RESOURCE` | Windows | N/A (Direct3D 12) |

### Texture Sharing Workaround

OIDN operates on linear buffers, not textures. To share Vulkan image data without a copy:
- Use `VK_IMAGE_TILING_LINEAR` for the Vulkan image.
- Share the backing buffer (not the image) via external memory.
- Pass explicit row stride (pixelStride / rowStride) to `oidnSetFilterImage`.

Caveat: `VK_IMAGE_TILING_LINEAR` has limited format support on many GPUs and is slower for rendering. In practice, a GPU-side copy from optimal-tiled image to a linear buffer is usually necessary (single `vkCmdCopyImageToBuffer` -- still far cheaper than a CPU roundtrip).

### Limitations

- No imported semaphore/fence support. Sync must happen on the host.
- CPU devices do not support external memory import.
- Not all GPU drivers support external memory. Always implement a host-copy fallback.
- macOS/Metal: Use `oidnNewSharedBufferFromMetal(device, mtlBuffer)` directly instead of FD/Win32 path.

---

## 3. Supported GPU Backends

| Backend | GPU Vendors | Platform | Notes |
|---------|-------------|----------|-------|
| **SYCL** | Intel (Xe, Xe2, Xe3: Arc, Iris Xe, Data Center) | Linux, Windows | Requires oneAPI DPC++ 6.2.1+ or oneAPI 2025.3+. Level Zero backend. |
| **CUDA** | NVIDIA (Turing, Ampere, Ada Lovelace, Hopper, Blackwell) | Linux, Windows | CUDA Toolkit 12.8+. CMake 3.18+. |
| **HIP** | AMD (RDNA 2, 3, 3.5, 4) | Linux, Windows | ROCm 6.4.2+. CMake 3.21+. |
| **Metal** | Apple (M1+) | macOS (ARM64 only) | Xcode 15.0+. CMake 3.21+. |
| **CPU** | Intel (SSE4.1+), ARM64 | All | ISPC 1.29.1+ and TBB 2017+ required to build. |

No native Vulkan compute backend exists. Vulkan renderers interop via external memory import (see section 2).

---

## 4. Build Requirements

### Core

- CMake 3.15+
- C++11 compiler (Clang, GCC, MSVC 2015+)
- Python 3

### Per-Backend Dependencies

| Backend | Dependencies |
|---------|-------------|
| CPU | ISPC 1.29.1+, Intel TBB 2017+ |
| SYCL | oneAPI DPC++ Compiler 6.2.1+ or Intel oneAPI 2025.3+ |
| CUDA | NVIDIA CUDA Toolkit 12.8+ |
| HIP | AMD ROCm 6.4.2+ |
| Metal | Xcode 15.0+ |

### CMake Options

```cmake
-DOIDN_DEVICE_CPU=ON       # default ON
-DOIDN_DEVICE_SYCL=OFF     # Intel GPUs
-DOIDN_DEVICE_CUDA=OFF     # NVIDIA GPUs
-DOIDN_DEVICE_HIP=OFF      # AMD GPUs
-DOIDN_DEVICE_METAL=OFF    # Apple GPUs (ARM64 macOS only)
-DOIDN_FILTER_RT=ON        # include RT filter weights
-DOIDN_FILTER_RTLIGHTMAP=ON # include lightmap filter weights
-DOIDN_APPS=ON             # build example apps
```

### Pre-built Binaries

Intel ships pre-built binaries for Linux/Windows/macOS at https://github.com/RenderKit/oidn/releases. These include CPU + the platform-relevant GPU backend. Linking against the shared library (`libOpenImageDenoise.so` / `OpenImageDenoise.dll`) is the simplest integration path.

---

## 5. Key Functions for Real-Time Game Use

### Quality Modes

| Mode | Enum | Use Case | Performance |
|------|------|----------|-------------|
| **Fast** | `OIDN_QUALITY_FAST` | Preview, lowest latency | ~1.5-2x faster than High |
| **Balanced** | `OIDN_QUALITY_BALANCED` | Interactive/real-time rendering | Good quality/perf tradeoff |
| **High** | `OIDN_QUALITY_HIGH` | Final frame, offline | Default, highest quality |

For a real-time game loop, use `OIDN_QUALITY_BALANCED` or `OIDN_QUALITY_FAST`.

### Async Execution Pipeline

```c
// Per-frame (after ray tracing pass writes to shared buffers):
oidnExecuteFilterAsync(filter);  // non-blocking GPU dispatch
// Do other CPU work here (game logic, audio, etc.)
oidnSyncDevice(device);          // block until denoise completes
```

### Memory Budget Control

```c
oidnSetFilterInt(filter, "maxMemoryMB", 256);  // cap VRAM usage
```

Useful for games targeting diverse hardware. Lower budget = potentially slower (more tiles) but won't OOM.

### Tile Constants

Query after `oidnCommitFilter`:
```c
int alignment = oidnGetFilterInt(filter, "tileAlignment");
int overlap   = oidnGetFilterInt(filter, "tileOverlap");
```
These are needed if manually splitting the frame into tiles (for lower latency or VRAM constraints).

### Filter Reuse

Critical for real-time: **do not recreate filters per frame**. Create once, commit once, execute many times. Rebuilding the filter recompiles the neural network and is expensive.

### Progress Monitor

```c
oidnSetFilterProgressMonitorFunction(filter, myCallback, userData);
```
Allows cancellation of long-running denoise operations (more relevant for offline).

### OIDN 3 (Coming H2 2026)

Will add **temporal denoising**: uses previous denoised frame + motion vectors + depth to reduce flickering. Two modes:
- Real-time: backward motion vectors only, minimal overhead.
- Final-quality: bidirectional, uses future frames.

Also moves to **kernel prediction** architecture (shared kernels across AOVs, supports supersampling), fixing current issues with overblurring and color shifts.

---

## 6. Integration Strategy for Godot

### Background

Godot 4.2+ removed its built-in OIDN integration (was CPU-only, single-threaded, slow) and replaced it with a JNLM compute shader denoiser for lightmaps. The JNLM denoiser is fast but not neural-network-based and differs in quality characteristics.

For Project Aurora, there are three viable integration paths:

### Option A: GDExtension + Shared Library (Recommended)

Build OIDN as a shared library, load it via a GDExtension (C++ native plugin).

**Pros:**
- No engine source modification.
- Full access to OIDN's C/C++ API including GPU backends and external memory.
- Can ship as a downloadable addon.

**Architecture:**
```
Godot Render Pass (Vulkan)
    |
    v
vkCmdCopyImageToBuffer (optimal-tiled RT output -> linear buffer)
    |
    v
Export buffer via VK_KHR_external_memory_fd / _win32
    |
    v
OIDN: import shared buffer, execute filter async
    |
    v
oidnSyncDevice (host sync)
    |
    v
Godot reads denoised buffer (shared memory, no copy back needed)
    |
    v
vkCmdCopyBufferToImage (linear buffer -> Vulkan texture for compositing)
```

**Implementation Steps:**
1. Create GDExtension with C++ bindings to OIDN.
2. On init: enumerate physical devices, match Godot's Vulkan device UUID to OIDN device.
3. Allocate exportable Vulkan buffers for color/albedo/normal/output.
4. Import them into OIDN once. Create and commit the filter once.
5. Each frame (or on-demand):
   - Copy Godot's render target to the shared linear buffer (GPU-side blit).
   - Fence/wait on Vulkan side.
   - `oidnExecuteFilterAsync` + `oidnSyncDevice`.
   - Copy denoised buffer back to a Vulkan texture.
   - Composite in Godot's post-processing chain.
6. Hook into Godot via **CompositorEffect** (Godot 4.3+): register a callback at `EFFECT_CALLBACK_TYPE_POST_TRANSPARENT` that triggers the denoise and composites the result.

**Fallback:** If external memory import is unsupported (CPU device, bad drivers), fall back to `oidnReadBuffer`/`oidnWriteBuffer` through host memory. Slower but universal.

### Option B: CompositorEffect with Subprocess

Run `oidnDenoise` CLI as an external process. Write frames to shared memory or temp files, read back results.

**Pros:** Zero native code, trivial to set up.
**Cons:** Latency from IPC/disk, not viable for real-time. Only useful for baked lightmaps or offline screenshots.

### Option C: Engine Fork

Patch Godot's rendering backend directly to call OIDN after the ray tracing pass.

**Pros:** Tightest integration, no buffer copies if you can access Vulkan internals directly.
**Cons:** Maintenance burden, must rebase on every Godot release. Not recommended unless the project diverges significantly from upstream Godot.

### Recommended Path

**Option A (GDExtension + CompositorEffect)** is the right call. It keeps the engine vanilla, ships as an addon, and the CompositorEffect API gives render-thread access to `RenderingDevice` for the Vulkan buffer operations. The main engineering cost is the Vulkan external memory plumbing, which is a one-time setup.

For lightmap-only denoising (not real-time), Option B is acceptable.

### Platform Matrix

| Platform | OIDN Backend | Vulkan Interop Method | Notes |
|----------|-------------|----------------------|-------|
| Linux + NVIDIA | CUDA | `OPAQUE_FD` | Best supported path |
| Linux + AMD | HIP | `OPAQUE_FD` or `DMA_BUF` | ROCm required |
| Linux + Intel | SYCL | `OPAQUE_FD` | oneAPI required |
| Windows + NVIDIA | CUDA | `OPAQUE_WIN32` | Well supported |
| Windows + AMD | HIP | `OPAQUE_WIN32` | Check driver support |
| macOS + Apple | Metal | `oidnNewSharedBufferFromMetal` | No Vulkan; use MoltenVK -> Metal buffer path |
| Any (fallback) | CPU | Host copy | Always works, ~50x slower than GPU |

---

## References

- Repository: https://github.com/RenderKit/oidn
- Documentation: https://www.openimagedenoise.org/documentation.html
- Releases / Pre-built binaries: https://github.com/RenderKit/oidn/releases
- Godot CompositorEffect docs: https://docs.godotengine.org/en/stable/tutorials/rendering/compositor.html
- Godot RenderingDevice API: https://docs.godotengine.org/en/stable/classes/class_renderingdevice.html
- Godot OIDN CLI proposal (closed): https://github.com/godotengine/godot-proposals/issues/7640
- OIDN 3 temporal denoising announcement: https://www.cgchannel.com/2026/01/open-image-denoise-3-will-support-temporal-denoising/
