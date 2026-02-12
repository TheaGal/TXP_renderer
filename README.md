# Thea Cross-Platform (TXP) Renderer

C++20 cross-platform renderer library.

## Usage

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


## Progress

Below is a chart showing the timeline of tasks.

```mermaid
gantt
    title TXP renderer progress
    dateFormat YYYY/MM/DD
    section Texture and material system
        Create shader pipeline from slang-reflection  : a1, 2026/02/08, 5d
        Bindlessly load all textures          : a2, after a1, 4d
        Material system                       : a3, after a2, 2d
        Material sets as swatches for models  : a4, after a8, 1d
    section Model system
        Load 3D meshes (gltf/obj)         : a5, after a3, 3d
        Material set from model tex names : a6, after a5, 1d
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
