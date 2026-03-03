# Thea Cross-Platform (TXP) Renderer

C++20 cross-platform renderer library.

## Usage

> NOTE: wavefront OBJ files must have the file extension ".wobj" instead of ".obj".

Below is a minimal example of setting up the renderer with 1 texture, 1 material, 1 material-set, 1 model.

```cpp
#include "txp_renderer_public.h"

#include <cstdint>


int32_t main()
{
    TXP::Renderer r{ "My renderer test!", 1280, 720 };

    r.add_texture("default_tex", ".ktx2");
    r.add_material("default_mat", "default_shader", { { "texture0", "default_tex" } });
    r.add_material_set("default_mat_set", { "default_mat" });
    r.add_model("default_model", ".glb", "default_mat_set");

    auto ro0_key = r.create_render_obj({
        .layer      = TXP::RENDER_LAYER_DEFAULT,
        .model_name = "default_model",
    });

    r.run();

    return 0;
}
```



## Software

- Clang
    - win64: 20.1.X (install llvm for windows and ninja)
    - macOS: 17.0.0 (default)

- Vulkan SDK
    > Include shader symbols.
    - win64: 1.4.341.0
    - macOS: 1.4.335.1 (note: contains MoltenVK 1.4.2)
        - System Global Installation

- vk-bootstrap v1.4.342

- VulkanMemoryAllocator v3.3.0


## Helper Software

### KTX-Sofware

Download from here (version 4.4.2): [click here](https://github.com/KhronosGroup/KTX-Software/releases/tag/v4.4.2)

For macOS, install the pkg. (uninstall with `sudo ktx-uninstall`)

For Windows, download the .exe and place it somewhere where it's available from PATH.


## Details

For Win64, macOS, and Linux, this renderer uses Vulkan 1.3 (MacOS being thru MoltenVK).

For other platforms, it is not planned yet.


## Memory usage

Below is a breakdown of the fixed costs of memory with this renderer (assuming a 1920x1080 resolution).

```mermaid
pie
    title Fixed Memory Costs at 1080p
    "HDR draw image (rgba16f)"             :   16588800
    "HDR draw image (d32s8)"               :   10368000
    "struct Environment_data (x3)"         :        444
    "struct Model_transform_set (x3)"      :   12582720
    "MipMapped 1k textures (rgba8) (x256)" : 2147483648
    "Remaining of 3GB of VRAM"             : 1034201860
```



## Progress

Below is a chart showing the timeline of tasks.

```mermaid
gantt
    title TXP renderer progress
    dateFormat YYYY/MM/DD
    section Texture and material system
        完 Create shader pipeline from slang-reflection  : a1, 2026/02/08, 10d
        Create shader pipeline from slang-reflection Pt2  : a1_1, after a8, 3d
        Bindlessly load all textures          : a2, after a1_1, 4d
        Material system                       : a3, after a2, 2d
        Material sets as swatches for models  : a4, after a3, 1d
    section Model system
        完 Load 3D meshes (gltf/obj)         : a5, after a1, 4d
        完 Geometry pipeline shader          : a5_1, after a5, 5d
        中 Camera controls                  : a5_2, after a5_1, 5d
        Material set from model tex names : a6, after a5_2, 1d
        Giant static model buffer         : a7, after a6, 2d
        Draw meshes with material system (use material set from model tex names)  : a8, after a7, 3d
    section Animation system
        Load .btafa and .btanitor to model : a9, after m1, 3d
        Compute shader of static mesh into skinned mesh in its own buffer : a10, after a9, 4d
        Control animators via setting state-sets : a11, after a10, 3d
        Control animators via jump queues : a12, after a10, 3d
    section Milestones
        Finish basic material-based geometry renderer : milestone, m1, after a4, 0d
        Finish animation system : milestone, m2, after a12, 0d
```


## Render Graph

Below is the flow of data for the render graph.

```mermaid
sequenceDiagram
    participant Shadow_images@{ "type" : "collections" }
    participant HDR_draw_image
    participant Volumetric_draw_image
    participant ImGui@{ "type" : "entity" }
    participant Swapchain@{ "type" : "collections" }
    participant Present@{ "type" : "queue" }
    Note over Shadow_images: Render shadow pass(es) (bindless)
    Shadow_images->>HDR_draw_image: Trans to shader read image
    Note over HDR_draw_image: Render geometry pass (bindless)
    HDR_draw_image->>Volumetric_draw_image: Copy depth texture
    Note over Volumetric_draw_image: Render volumetric pass (low res)
    Note over HDR_draw_image: Render skybox pass
    Volumetric_draw_image->>Volumetric_draw_image: Trans to shader read image
    Volumetric_draw_image->>HDR_draw_image: Upscale onto main image
    HDR_draw_image-->>ImGui: Tonemap to LDR
    ImGui->>Swapchain: Render editor GUI
    HDR_draw_image-->>Swapchain: Tonemap to LDR (if not rendering into ImGui)
    Swapchain->>Present: Present
```
