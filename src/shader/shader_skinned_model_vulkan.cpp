#if TXP_GFX_BACKEND_VULKAN

// clang-format off
#include "shader_skinned_model.h"
// clang-format on

#include "btlogger.h"
#include "material_organizer/material_organizer.h"
#include "renderer/gfx_vulkan/vk_image.h"
#include "renderer/gfx_vulkan_impl.h"
#include "shader/shader_support.h"
#include "shader_creation/shader_creation.h"
#include "vulkan/vulkan_core.h"

#include <memory>
#include <stdexcept>


namespace TXP
{
namespace Shader
{

// struct Shader_skinned_model::Impl
struct Shader_skinned_model::Impl
{
    Impl(TXP::Material_organizer& mat_coll, TXP::Graphics::Impl& graphics)
        : material_organizer(mat_coll)
        , g(graphics)
        , device(g.gfx.device)
    {
        material_organizer.emplace_shader(k_name);

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
            },
            VK_SHADER_STAGE_COMPUTE_BIT,
            0);

        // Descriptors.
        shader_pipeline.descriptor_set =
            g.global_descriptor_allocator.allocate(shader_pipeline.descriptor_layout);

        VkDescriptorImageInfo img_info{
            .imageView = g.render_views[0].color_image.get_image_view(),
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        };

        VkWriteDescriptorSet img_write{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,

            .dstSet = shader_pipeline.descriptor_set,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &img_info,
        };

        vkUpdateDescriptorSets(device, 1, &img_write, 0, nullptr);

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Pipeline.

        VkResult err;

        // Create pipeline layout.
        VkPipelineLayoutCreateInfo pipeline_layout_info{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .setLayoutCount = 1,
            .pSetLayouts = &shader_pipeline.descriptor_layout,
            .pushConstantRangeCount = 0,
            .pPushConstantRanges = nullptr,
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


    TXP::Material_organizer& material_organizer;

    TXP::Graphics::Impl& g;
    VkDevice device;

    std::string compute_entry_point_name;
    VkExtent3D thread_grp_sizes;

    /// Shader pipeline info for this shader.
    struct Shader_pipeline
    {
        VkPipeline pipeline;
        VkPipelineLayout pipeline_layout;
        VkDescriptorSet descriptor_set;
        VkDescriptorSetLayout descriptor_layout;
    } shader_pipeline;
};


// class Shader_skinned_model
Shader_skinned_model::Shader_skinned_model(Material_organizer& material_organizer, void* graphics)
    : m_pimpl(
          std::make_unique<Impl>(material_organizer, *static_cast<TXP::Graphics::Impl*>(graphics)))
{
}

Shader_skinned_model::~Shader_skinned_model() = default;

void Shader_skinned_model::make_material(
    std::string const& material_name,
    std::unordered_map<std::string, std::string> const& shader_params)
{
    // Do nothing.
    BT_WARN("This material has no shader params.");

    m_pimpl->material_organizer.emplace_material(material_name, k_name);
}

void Shader_skinned_model::organize_materials()
{
    // Do nothing.
    BT_TRACE("Shader_skinned_model has no material collection.");
}

void Shader_skinned_model::allocate_per_instance_data_slots(
    std::vector<Render_object> const& render_object_list,
    std::vector<Render_object_model_mesh_reference>& out_model_mesh_ref_list,
    size_t& in_out_cur_modmesh_ref_idx)
{
    // Do nothing.
    // BT_TRACE("Shader_skinned_model has no per-instance data.");
}

void Shader_skinned_model::compute(void* render_frame)
{
    auto& p{ *m_pimpl };

    auto cmd{ p.g.get_current_frame().graphics_queue_command_buffer.get() };

    Vk_Image::Image::transition_to(
        cmd,
        { { &p.g.render_views[0].color_image.get_image(), VK_IMAGE_LAYOUT_GENERAL } });

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, p.shader_pipeline.pipeline);
    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_COMPUTE,
                            p.shader_pipeline.pipeline_layout,
                            0,
                            1, &p.shader_pipeline.descriptor_set,
                            0, nullptr);

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

    k_cmd_dispatch_fn(cmd, p.g.render_views[0].color_image.get_extent(), p.thread_grp_sizes);
}

}  // namespace Shader
}  // namespace TXP

#endif // TXP_GFX_BACKEND_VULKAN
