#if TXP_GFX_BACKEND_VULKAN

#include "gfx.h"

#include "gfx_vulkan_impl.h"

// vv Must be in this order vv
// clang-format off
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
// clang-format on

#include "animation_frame_action/runtime_data.h"
#include "btdatecheck.h"
#include "btlogger.h"
#include "gfx_vulkan/vk_image.h"
#include "material_organizer/material_organizer.h"
#include "render_object/render_model.h"
#include "renderer/types.h"

#include <cassert>
#include <stdexcept>
#include <string>


// class Graphics
TXP::Graphics::Graphics(std::string const& title,
                        int32_t width,
                        int32_t height,
                        Information_hook_struct info_hook_struct)
    : m_pimpl(std::make_unique<Impl>(title, width, height, info_hook_struct))
{
    m_pimpl->init_glfw_no_api();
    m_pimpl->init_window_props();
    m_pimpl->init_window();
    m_pimpl->init_vulkan_instance();
    m_pimpl->init_vulkan_window_surface();
    m_pimpl->init_vulkan_build_device();
    m_pimpl->init_vulkan_create_memory_allocator();
    m_pimpl->select_vulkan_window_surface_format();
    m_pimpl->init_vulkan_build_swapchain();
    m_pimpl->init_vulkan_retrieve_queues();
    m_pimpl->init_vulkan_create_cmd_structures();
    m_pimpl->init_vulkan_create_sync_structures();
    m_pimpl->init_vulkan_for_imgui();
    m_pimpl->init_vulkan_render_graph_resources();
    m_pimpl->init_vulkan_create_descriptors();
}

TXP::Graphics::~Graphics()
{
    m_pimpl->destroy_texture_entries();
    m_pimpl->destroy_vulkan();
    m_pimpl->destroy_glfw();
}

void TXP::Graphics::load_texture_assets(std::string const& texture_asset_dir,
                                        std::vector<Texture_asset_create_info>&& texture_assets)
{   // Load textures.
    m_pimpl->construct_ktx_vk_device_info();
    for (auto const& tex_asset : texture_assets)
    {
        m_pimpl->add_texture_entry(
            tex_asset.texture_name,
            m_pimpl->load_and_upload_texture(texture_asset_dir + tex_asset.ktx2_fname));
    }
    m_pimpl->destruct_ktx_vk_device_info();
    BT_TRACEF("Loaded all %zu textures.", texture_assets.size());

    // Create "all textures" descriptor.
    // @NOTE: required before shaders are initialized!!! (order importance)
    m_pimpl->create_all_textures_descriptor();
    BT_TRACE("Created all-textures descriptor.");
}

void TXP::Graphics::load_material_palettes(
    std::vector<Material_asset_create_info>&& material_assets,
    std::vector<Material_palette_asset_create_info>&& material_palette_assets,
    Material_organizer& material_organizer)
{
    for (auto const& mat_pal_asset : material_palette_assets)
    {
        Material_palette new_mat_pal;
        new_mat_pal.emplace_materials(material_organizer, mat_pal_asset.materials);
        material_organizer.emplace_material_palette(mat_pal_asset.mat_set_name,
                                                    std::move(new_mat_pal));
    }
    BT_TRACEF("Loaded all %zu material palettes.", material_palette_assets.size());
}

void TXP::Graphics::load_model_assets(std::string const& afa_asset_dir,
                                      std::vector<Model_asset_create_info>&& model_assets,
                                      Render_model_data_collection& render_model_data_collection,
                                      Material_organizer& material_organizer)
{   // Load models.
    for (auto const& mod_asset : model_assets)
    {
        load_model_from_disk(render_model_data_collection,
                             material_organizer,
                             mod_asset.model_name,
                             mod_asset.file_ext);
    }
    BT_TRACEF("Loaded all %zu models.", model_assets.size());

    render_model_data_collection.lock_in_number_of_static_models();
    m_pimpl->upload_model_entries_to_gpu(render_model_data_collection);
    BT_TRACE("Uploaded combined model to GPU.");

    // Load animators and anim frame actions.
    size_t num_afas_loaded{ 0 };
    for (auto const& mod_asset : model_assets)
    {
        if (mod_asset.load_animator_template)
        {
            // @NOTE: animator template is lazy-loaded. No pre-loading at this time.

            if (mod_asset.load_anim_frame_action)
            {   // Pre-load in AFA.
                anim_frame_action::Bank::emplace(
                    mod_asset.model_name,
                    anim_frame_action::Runtime_data_controls{ afa_asset_dir + mod_asset.model_name +
                                                              ".btafa" });
                num_afas_loaded++;
            }
        }
        else if (mod_asset.load_anim_frame_action)
            throw std::runtime_error("Requires loading animator template if wanting to load AFA.");
    }
    BT_TRACEF("Loaded all %zu anim frame action files.", num_afas_loaded);
}

void TXP::Graphics::poll_input_events()
{
    m_pimpl->poll_input_events();
}

void TXP::Graphics::build_imgui_contents(Camera_internal& camera,
                                         std::vector<Render_view_size>& out_rend_view_sizes)
{
    m_pimpl->build_imgui_contents(camera, out_rend_view_sizes);
}

bool TXP::Graphics::check_render_view_sizes_changed(
    std::vector<Render_view_size> const& rend_view_sizes) const
{
    return m_pimpl->check_render_view_sizes_changed(rend_view_sizes);
}

void TXP::Graphics::set_render_view_sizes(std::vector<Render_view_size> const& rend_view_sizes)
{
    m_pimpl->set_render_view_sizes(rend_view_sizes);
}

void TXP::Graphics::set_render_view_camera(size_t render_view_idx, mat4 camera_projection, mat4 camera_view)
{
    m_pimpl->set_render_view_camera(render_view_idx, camera_projection, camera_view);
}

void TXP::Graphics::set_directional_light(size_t render_view_idx,
                                          vec3 direction,
                                          vec3 color,
                                          float_t intensity)
{
    m_pimpl->set_directional_light(render_view_idx, direction, color, intensity);
}

void* TXP::Graphics::get_render_view(size_t rend_view_idx)
{
    return m_pimpl->get_render_view(rend_view_idx);
}

void TXP::Graphics::begin_rendering_render_view(size_t rend_view_idx)
{
    m_pimpl->begin_rendering_render_view(rend_view_idx);
}

void TXP::Graphics::end_rendering_render_view(size_t /*rend_view_idx*/)
{
    m_pimpl->end_rendering_render_view();
}

void TXP::Graphics::set_render_object_per_instance_data(
    Material_organizer const& material_organizer,
    std::vector<Render_object> const& rend_obj_list,
    std::vector<Render_object_model_mesh_reference> const& model_mesh_ref_list,
    size_t mod_mesh_ref_list_length)
{
    m_pimpl->set_render_object_per_instance_data(material_organizer,
                                                 rend_obj_list,
                                                 model_mesh_ref_list,
                                                 mod_mesh_ref_list_length);
}

void TXP::Graphics::compute_light_culling()
{
    assert(false);
}

void TXP::Graphics::compute_shadow_culling()
{
    assert(false);
}

void TXP::Graphics::compute_opaque_geometry_culling()
{
    assert(false);
}

void TXP::Graphics::compute_transparent_geometry_culling()
{
    assert(false);
}

void TXP::Graphics::render_shadows()
{
    assert(false);
}

void TXP::Graphics::render_clouds()
{
    assert(false);
}

void TXP::Graphics::render_volumetric_light()
{
    assert(false);
}

void TXP::Graphics::render_particles()
{
    assert(false);
}

void TXP::Graphics::render_transparent_geometry()
{
    assert(false);
}

void TXP::Graphics::render_hdr_to_ldr_postprocessing(size_t rend_view_idx, Ldr_target render_target)
{
    // @TEMPORARY: this is only a blit or an image transition, but in the future have real
    //             tonemapping.
    switch (render_target)
    {
    case LDR_TARGET_SWAPCHAIN:
    {
        auto const& swapchain_extent{ m_pimpl->gfx.swapchain_extent };
        m_pimpl->blit_image(m_pimpl->render_views[rend_view_idx].color_image.get_image(),
                            m_pimpl->render_views[rend_view_idx].color_image.get_extent(),
                            m_pimpl->gfx.swapchain_images[m_pimpl->current_swapchain_image_idx],
                            VkExtent3D{ .width = swapchain_extent.width,
                                        .height = swapchain_extent.height,
                                        .depth = 1 });
        break;
    }

    case LDR_TARGET_IMGUI:
    {
        Vk_Image::Image::transition_to(
            m_pimpl->get_current_frame().graphics_queue_command_buffer.get(),
            { { &m_pimpl->render_views[rend_view_idx].color_image.get_image(),
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL } });
        break;
    }
    }
}

void TXP::Graphics::render_imgui()
{
    m_pimpl->render_imgui();
}

bool TXP::Graphics::start_next_frame()
{
    return m_pimpl->start_next_frame();
}

void TXP::Graphics::present_frame_to_screen()
{
    m_pimpl->present_frame_to_screen();
}

void TXP::Graphics::wait_until_gpu_idle()
{
    m_pimpl->wait_until_gpu_idle();
}

TXP::Graphics::Impl* TXP::Graphics::get_impl()
{
    return m_pimpl.get();
}

#endif // TXP_GFX_BACKEND_VULKAN
