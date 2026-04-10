#if TXP_GFX_BACKEND_VULKAN

// clang-format off
#include "shader_skinned_model.h"
// clang-format on

#include "btlogger.h"
#include "material_organizer/material_organizer.h"
#include "render_object/deformed_render_model.h"
#include "render_object/render_model.h"
#include "render_object/render_object.h"
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

/// Struct for push constants.
struct Shader_skinned_model_push_constants
{
    VkDeviceAddress input_model_dev_addr;
    VkDeviceAddress skin_data_coll_dev_addr;
    VkDeviceAddress joint_trans_coll_dev_addr;
    uint32_t input_model_vertex_start;
    uint32_t output_model_vertex_start;
    uint32_t num_model_vertices;
};

// struct Shader_skinned_model::Impl
struct Shader_skinned_model::Impl
{
    Impl(TXP::Material_organizer& mat_coll,
         TXP::Render_model_data_collection& rmd_coll,
         TXP::Graphics::Impl& graphics)
        : material_organizer(mat_coll)
        , rend_model_data_coll(rmd_coll)
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
                { 0, { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER } },
            },
            VK_SHADER_STAGE_COMPUTE_BIT,
            0);

        assert(false);  // Do vv below vv
        #if 0  // @TODO: @THEA: put this in its own func that gets called whenever the combined deformed model buffer is rebuilt!!!  -Thea 2026/04/08
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
        #endif // 0  // @TODO: @THEA: put this in its own func that gets called whenever the combined deformed model buffer is rebuilt!!!  -Thea 2026/04/08

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Pipeline.

        VkResult err;

        // Create pipeline layout.
        VkPushConstantRange push_constant_range{ .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                                 .size =
                                                     sizeof(Shader_skinned_model_push_constants) };
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


    TXP::Material_organizer& material_organizer;
    TXP::Render_model_data_collection& rend_model_data_coll;

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
Shader_skinned_model::Shader_skinned_model(Material_organizer& material_organizer,
                                           Render_model_data_collection& rend_model_data_coll,
                                           void* graphics)
    : m_pimpl(std::make_unique<Impl>(material_organizer,
                                     rend_model_data_coll,
                                     *static_cast<TXP::Graphics::Impl*>(graphics)))
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

void Shader_skinned_model::build_combined_deformed_vertex_set_descriptor_set()
{
    assert(false);  // @TODO: @HERE: Create descriptor set for the combined deformed vertex
                    //   set (and keep it updated whenever that combined model changes or
                    //   gets rebuilt).
}

void Shader_skinned_model::compute(void* deformed_model_ptr)
{
    auto& p{ *m_pimpl };

    auto cmd{ p.g.get_current_frame().graphics_queue_command_buffer.get() };

    auto const& deformed_model{ *static_cast<Deformed_model_data_set*>(deformed_model_ptr) };

    // @THEA: @TODO: Is transitioning the buffer necessary in this case???
    // Vk_Image::Image::transition_to(
    //     cmd,
    //     { { &p.g.render_views[0].color_image.get_image(), VK_IMAGE_LAYOUT_GENERAL } });

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, p.shader_pipeline.pipeline);
    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_COMPUTE,
                            p.shader_pipeline.pipeline_layout,
                            0,
                            1, &p.shader_pipeline.descriptor_set,
                            0, nullptr);

    // Get base static model of deformed model.
    auto const& base_static_model{ p.rend_model_data_coll.get_static_model_data_set(
        deformed_model.base_static_model_idx) };

    // Push constants.
    Shader_skinned_model_push_constants push_consts{
        .input_model_dev_addr =
            p.g.combined_static_model.vertex_index_buffer.get_device_address(),
        .skin_data_coll_dev_addr =
            deformed_model.model_skin.vert_skin_data_buffer.get_device_address(),
        .joint_trans_coll_dev_addr =
            deformed_model.joint_transforms_buffer.get_device_address(),
        .input_model_vertex_start =
            static_cast<uint32_t>(base_static_model.vertex_index_offset),
        .output_model_vertex_start =
            static_cast<uint32_t>(deformed_model.deformed_model.vertex_index_offset),
        .num_model_vertices =
            static_cast<uint32_t>(base_static_model.vertices.size()),
    };
    vkCmdPushConstants(cmd,
                       p.shader_pipeline.pipeline_layout,
                       VK_SHADER_STAGE_COMPUTE_BIT,
                       0,
                       sizeof(Shader_skinned_model_push_constants),
                       &push_consts);

    // @TODO: move this into a real function.
    static auto const k_cmd_dispatch_fn =
        [](VkCommandBuffer cmd, VkExtent3D dispatch_thread_sizes, VkExtent3D thread_group_sizes) {
            vkCmdDispatch(cmd,
                          std::max(1u,
                                   (dispatch_thread_sizes.width + thread_group_sizes.width - 1) /
                                       thread_group_sizes.width),
                          std::max(1u,
                                   (dispatch_thread_sizes.height + thread_group_sizes.height - 1) /
                                       thread_group_sizes.height),
                          std::max(1u,
                                   (dispatch_thread_sizes.depth + thread_group_sizes.depth - 1) /
                                       thread_group_sizes.depth));
        };

    k_cmd_dispatch_fn(cmd,
                      VkExtent3D{
                          .width = push_consts.num_model_vertices,
                          .height = 0,
                          .depth = 0,
                      },
                      p.thread_grp_sizes);
}

}  // namespace Shader
}  // namespace TXP

#endif // TXP_GFX_BACKEND_VULKAN
