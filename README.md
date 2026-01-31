# Thea Cross-Platform (TXP) Renderer

C++20 cross-platform renderer library.





## Software

- Clang
    - win64: 20.1.X (install llvm for windows and ninja)
    - macOS: 17.0.0 (default)

- Vulkan SDK
    - System Global Installation
    - win64: 1.3.296.0
        > @NOTE: a different version could be more suitable? Still unknown how much the 1.3 vs 1.4 vulkan sdks change development.
    - macOS: 1.4.335.1 (note: contains MoltenVK 1.4.2)

- vk-bootstrap v1.4.341

- VulkanMemoryAllocator v3.3.0


## Details

For Win64, macOS, and Linux, this renderer uses Vulkan 1.3 (MacOS being thru MoltenVK).

For other platforms, it is not planned yet.
