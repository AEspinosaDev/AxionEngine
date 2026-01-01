# Axion Engine Samples 🧪

This directory contains a collection of samples demonstrating how to use the different modules of the Axion Engine. 

The samples are organized by **Module Dependency**, allowing you to learn the engine layers progressively, from low-level graphics to high-level scene management.

## 📂 Directory Structure

### 1. 🟢 GFX (Pure Graphics)
**Prefix:** `GFX`  
**Dependencies:** `AxionCommon`, `AxionGFX`

Samples that demonstrate the **Low-Level Rendering Framework**. These examples do not use the Core module (no Assets, no ECS, no Scene). They strictly show how to use the RHI, RenderGraph, and Graphic/RTX/Compute pipelines. Ideal for Graphics Programmers.

* **`GFX_Triangle`**: The "Hello World" of graphics. Basic raster pipeline setup.
* **`GFX_Compute`**: A compute shader example with UAVs and Structured Buffers.
* **`GFX_Raytracing`**: A real-time path-tracing example.


---

### 2. 🔵 Core (System & Logic)
**Prefix:** `Core`  
**Dependencies:** `AxionCommon`, `AxionCore`

Samples focused on OS interaction, file systems, and engine subsystems without rendering complex graphics.

* **(WIP) Core_Scene**: Creation and managing of a Scene.


---

### 3. 🟠 Interop (Integration Workflows)
**Prefix:** `Interop`  
**Dependencies:** `AxionCommon`, `AxionCore`, `AxionGFX`

**✨ The Sweet Spot for Prototyping.** These samples demonstrate how to make the modules talk to each other. Specifically, how to use `AxionCore` as a Resource Loader to feed data into the `AxionGFX` renderer, bypassing the high-level Scene Graph.

* **`Interop_RaytracingAjax`**: Loads a complex OBJ mesh (Ajax) using the Core AssetManager and renders it using a Path Tracing pipeline in GFX. Shows manual data bridging between CPU (Core) and GPU (GFX).

---

### 4. 🟣 Demos (High Level)
**Prefix:** `Demo`  
**Dependencies:** `All Modules`

Full applications utilizing the entire engine stack: Scene Graph, ECS, Renderer, and Scripting.

* **(WIP) Demo_RenderEngine**: A complete viewer application.
* **(WIP) Demo_GameSample**: A small game logic test.

---

## 🛠️ How to Build

All samples are enabled by default in CMake. To build them:

```bash
cd build
cmake -DAXION_BUILD_SAMPLES=ON ..
cmake --build .