# Thea Cross-Platform (TXP) Renderer

C++20 cross-platform renderer library.





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


## Details

For Win64, macOS, and Linux, this renderer uses Vulkan 1.3 (MacOS being thru MoltenVK).

For other platforms, it is not planned yet.
