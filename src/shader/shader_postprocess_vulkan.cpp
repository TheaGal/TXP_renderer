#if TXP_GFX_BACKEND_VULKAN

// clang-format off
#include "shader_postprocess.h"
// clang-format on

#include "btlogger.h"
#include "renderer/gfx_vulkan/vk_image.h"
#include "renderer/gfx_vulkan_impl.h"
#include "shader/shader_support.h"
#include "shader_creation/shader_creation.h"
#include "vulkan/vulkan_core.h"

#include <memory>
#include <stdexcept>
#include <unordered_map>


namespace TXP
{
namespace Shader
{

/// Struct for push constants.
struct Shader_postprocess_push_constants
{
    uint32_t use_ui_image;
    float_t exposure;
    float_t gamma;
};

// struct Shader_postprocess::Impl
struct Shader_postprocess::Impl
{
    Impl(TXP::Graphics::Impl& graphics)
        : g(graphics)
        , device(g.gfx.device)
    {
        Shader_Support::fetch_compute_shader_info(k_name,
                                                  compute_entry_point_name,
                                                  thread_grp_sizes.width,
                                                  thread_grp_sizes.height,
                                                  thread_grp_sizes.depth);


        // @TODO: for vv below vv pull out the `build_Descriptor_layout()` and
        //        `load_shader_module()` functions.

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Descriptors.

        // Descriptor layouts.
        shader_pipeline.descriptor_layout = g.build_descriptor_layout(
            {
                { 0, { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE } },
                { 1, { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE } },
                { 2, { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE } },
            },
            VK_SHADER_STAGE_COMPUTE_BIT,
            0);

        // @TODO: @THEA: vv delete this vv
        // // Descriptors.
        // shader_pipeline.descriptor_set =
        //     g.global_descriptor_allocator.allocate(shader_pipeline.descriptor_layout);

        // VkDescriptorImageInfo img_info{
        //     .imageView = g.render_views[0].color_image.get_image_view(),
        //     .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        // };

        // VkWriteDescriptorSet img_write{
        //     .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        //     .pNext = nullptr,

        //     .dstSet = shader_pipeline.descriptor_set,
        //     .dstBinding = 0,
        //     .descriptorCount = 1,
        //     .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        //     .pImageInfo = &img_info,
        // };

        // vkUpdateDescriptorSets(device, 1, &img_write, 0, nullptr);

        // Defer building descriptors.
        shader_pipeline.is_descriptor_set_valid = false;

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Pipeline.

        VkResult err;

        // Create pipeline layout.
        VkPushConstantRange push_constant_range{ .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                                 .size =
                                                     sizeof(Shader_postprocess_push_constants) };
        VkPipelineLayoutCreateInfo pipeline_layout_info{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .setLayoutCount = 1,
            .pSetLayouts = &shader_pipeline.descriptor_layout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &push_constant_range,
        };

        err = vkCreatePipelineLayout(device,
                                     &pipeline_layout_info,
                                     nullptr,
                                     &shader_pipeline.pipeline_layout);
        if (err)
            throw std::runtime_error("Failed to create pipeline layout.");

        // Create pipeline.
        VkShaderModule shader_module{ g.load_shader_module(
            Shader_Creation::get_shader_module_path(k_name)) };

        VkPipelineShaderStageCreateInfo stage_info{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = shader_module,
            .pName = compute_entry_point_name.c_str(),
            // .pSpecializationInfo = nullptr,  // @RESEARCH: research this if you want!
        };

        VkComputePipelineCreateInfo compute_pipeline_info{
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .pNext = nullptr,
            .stage = stage_info,
            .layout = shader_pipeline.pipeline_layout,
        };

        err = vkCreateComputePipelines(device,
                                       VK_NULL_HANDLE,
                                       1,
                                       &compute_pipeline_info,
                                       nullptr,
                                       &shader_pipeline.pipeline);
        if (err)
            throw std::runtime_error("Failed to create compute pipeline.");

        // Cleanup.
        vkDestroyShaderModule(device, shader_module, nullptr);
    }

    ~Impl()
    {
        vkDestroyPipelineLayout(device, shader_pipeline.pipeline_layout, nullptr);
        vkDestroyPipeline(device, shader_pipeline.pipeline, nullptr);
    }

    void build_descriptors()
    {
        auto& sp{ shader_pipeline };

        // Reallocate.
        size_t num_render_views{ g.render_views.size() };

        sp.per_render_view_data_map.clear();
        sp.per_render_view_data_map.reserve(num_render_views);

        sp.destination_images.clear();
        sp.destination_images.reserve(num_render_views);

        // Build descriptors.
        size_t render_view_idx{ 0 };
        for (auto& render_view : g.render_views)
        {
            // Create map entry.
            std::vector<Vk_Image::Image*> descriptor_set_images;
            descriptor_set_images.reserve(render_view_idx == 0 ? 3 : 2);
            descriptor_set_images.emplace_back(&render_view.color_image.get_image());
            descriptor_set_images.emplace_back(&render_view.destination_image.get_image());
            if (render_view_idx == 0)
                descriptor_set_images.emplace_back(&g.ui_image.get_image());

            sp.per_render_view_data_map[&render_view] = {
                .descriptor_set_idx = static_cast<uint32_t>(render_view_idx),
                .descriptor_set_images = descriptor_set_images,
            };

            // Allocate needed descriptors.
            while (sp.allocated_descriptor_sets.size() < sp.per_render_view_data_map.size())
            {
                sp.allocated_descriptor_sets.emplace_back(
                    g.global_descriptor_allocator.allocate(sp.descriptor_layout));
            }

            // Write descriptors.
            VkDescriptorSet dest_descriptor_set{ sp.allocated_descriptor_sets[render_view_idx] };

            VkDescriptorImageInfo img_info_0{
                .imageView = render_view.color_image.get_image_view(),
                .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
            };
            VkWriteDescriptorSet img_write_0{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = nullptr,

                .dstSet = dest_descriptor_set,
                .dstBinding = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .pImageInfo = &img_info_0,
            };

            VkDescriptorImageInfo img_info_1{
                .imageView = g.ui_image.get_image_view(),
                .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
            };
            VkWriteDescriptorSet img_write_1{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = nullptr,

                .dstSet = dest_descriptor_set,
                .dstBinding = 1,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .pImageInfo = &img_info_1,
            };

            VkDescriptorImageInfo img_info_2{
                .imageView = render_view.destination_image.get_image_view(),
                .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
            };
            VkWriteDescriptorSet img_write_2{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = nullptr,

                .dstSet = dest_descriptor_set,
                .dstBinding = 2,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .pImageInfo = &img_info_2,
            };

            VkWriteDescriptorSet img_writes[]{ img_write_0, img_write_1, img_write_2 };

            vkUpdateDescriptorSets(device,
                                   sizeof(img_writes) / sizeof(VkWriteDescriptorSet),
                                   img_writes,
                                   0,
                                   nullptr);

            // Destination image entry.
            sp.destination_images.emplace_back(&render_view.destination_image.get_image());

            render_view_idx++;
        }

        // Finished.
        sp.is_descriptor_set_valid = true;
    }


    TXP::Graphics::Impl& g;
    VkDevice device;

    std::string compute_entry_point_name;
    VkExtent3D thread_grp_sizes;

    /// Shader pipeline info for this shader.
    struct Shader_pipeline
    {
        VkPipeline pipeline;
        VkPipelineLayout pipeline_layout;
        VkDescriptorSetLayout descriptor_layout;

        std::vector<VkDescriptorSet> allocated_descriptor_sets;

        struct Per_render_view_data
        {
            uint32_t descriptor_set_idx;
            std::vector<Vk_Image::Image*> descriptor_set_images;
        };
        std::unordered_map<Graphics::Impl::Render_view_data*, Per_render_view_data>
            per_render_view_data_map;
        std::vector<Vk_Image::Image*> destination_images;
        bool is_descriptor_set_valid;
    } shader_pipeline;
};


// class Shader_postprocess
Shader_postprocess::Shader_postprocess(void* graphics)
    : m_pimpl(std::make_unique<Impl>(*static_cast<TXP::Graphics::Impl*>(graphics)))
{
}

Shader_postprocess::~Shader_postprocess() = default;

void Shader_postprocess::signal_render_view_sizes_changed()
{
    m_pimpl->shader_pipeline.is_descriptor_set_valid = false;
}

// @TODO: @THEA: remove the `render_target` param since now there's just going to be rendering to a destination image.
void Shader_postprocess::compute(bool use_ui_image, void* render_view_param)
{
    auto& p{ *m_pimpl };

    if (!p.shader_pipeline.is_descriptor_set_valid)
    {
        p.build_descriptors();
    }

    auto cmd{ p.g.get_current_frame().graphics_queue_command_buffer.get() };

    auto& render_view{ *static_cast<Graphics::Impl::Render_view_data*>(render_view_param) };

    // Ready needed images for compute shader.
    auto const& prv_data{ p.shader_pipeline.per_render_view_data_map.at(&render_view) };

    std::vector<std::pair<Vk_Image::Image*, VkImageLayout>> image_transitions;
    image_transitions.reserve(prv_data.descriptor_set_images.size());
    for (auto* desc_set_image : prv_data.descriptor_set_images)
    {
        image_transitions.emplace_back(desc_set_image, VK_IMAGE_LAYOUT_GENERAL);
    }

    Vk_Image::Image::transition_to(cmd, std::move(image_transitions));

    // Bind.
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, p.shader_pipeline.pipeline);
    vkCmdBindDescriptorSets(
        cmd,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        p.shader_pipeline.pipeline_layout,
        0,
        1, &p.shader_pipeline.allocated_descriptor_sets[prv_data.descriptor_set_idx],
        0, nullptr);

    Shader_postprocess_push_constants push_consts{
        .use_ui_image = use_ui_image,
        .exposure = 1,
        .gamma = 1,
    };
    vkCmdPushConstants(cmd,
                       p.shader_pipeline.pipeline_layout,
                       VK_SHADER_STAGE_COMPUTE_BIT,
                       0,
                       sizeof(Shader_postprocess_push_constants),
                       &push_consts);

    // Run.
    // @TODO: move this into a real function.
    static auto const k_cmd_dispatch_fn =
        [](VkCommandBuffer cmd, VkExtent3D dispatch_thread_sizes, VkExtent3D thread_group_sizes) {
            vkCmdDispatch(cmd,
                          (dispatch_thread_sizes.width + thread_group_sizes.width - 1) /
                              thread_group_sizes.width,
                          (dispatch_thread_sizes.height + thread_group_sizes.height - 1) /
                              thread_group_sizes.height,
                          (dispatch_thread_sizes.depth + thread_group_sizes.depth - 1) /
                              thread_group_sizes.depth);
        };

    k_cmd_dispatch_fn(cmd, render_view.color_image.get_extent(), p.thread_grp_sizes);
}

void Shader_postprocess::wait_until_completion()
{
    auto& p{ *m_pimpl };

    assert(p.shader_pipeline.is_descriptor_set_valid);

    auto cmd{ p.g.get_current_frame().graphics_queue_command_buffer.get() };

    std::vector<std::pair<Vk_Image::Image*, VkImageLayout>> image_transitions;
    image_transitions.reserve(p.shader_pipeline.destination_images.size());
    for (auto* dest_image : p.shader_pipeline.destination_images)
    {
        image_transitions.emplace_back(dest_image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    Vk_Image::Image::transition_to(cmd, std::move(image_transitions));
}

}  // namespace Shader
}  // namespace TXP

#endif // TXP_GFX_BACKEND_VULKAN
