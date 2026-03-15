#if TXP_GFX_BACKEND_VULKAN

#include "gfx.h"

#include "gfx_vulkan_impl.h"

// vv Must be in this order vv
// clang-format off
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
// clang-format on

#include "btdatecheck.h"
#include "btlogger.h"
#include "gfx_vulkan/vk_image.h"
#include "material_collection/material_collection.h"
#include "render_object/render_model.h"
#include "types.h"

#include <cassert>
#include <string>


// class Graphics
TXP::Graphics::Graphics(std::string const& title, int32_t width, int32_t height)
    : m_pimpl(std::make_unique<Impl>(title, width, height))
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
    // m_pimpl->destroy_texture_entries();  @TODO

    // @TODO
    assert(false);
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
}

void TXP::Graphics::load_material_palettes(
    std::vector<Material_asset_create_info>&& material_assets,
    std::vector<Material_set_asset_create_info>&& material_set_assets,
    Material_collection& material_collection)
{
    for (auto const& mat_pal_asset : material_set_assets)
    {
        Material_palette new_mat_pal;
        new_mat_pal.emplace_materials(material_collection, mat_pal_asset.materials);
        material_collection.emplace_material_palette(mat_pal_asset.mat_set_name,
                                                     std::move(new_mat_pal));
    }
    BT_TRACEF("Loaded all %zu material palettes.", material_set_assets.size());
}

void TXP::Graphics::load_model_assets(std::vector<Model_asset_create_info>&& model_assets,
                                      Render_model_data_collection& render_model_data_collection,
                                      Material_collection& material_collection)
{   // Load models.
    for (auto const& mod_asset : model_assets)
    {
        load_model_from_disk(render_model_data_collection,
                             material_collection,
                             mod_asset.model_name,
                             mod_asset.file_ext);
    }
    BT_TRACEF("Loaded all %zu models.", model_assets.size());

    m_pimpl->upload_model_entries_to_gpu(render_model_data_collection);
    BT_TRACE("Uploaded combined model to GPU.");
}

void TXP::Graphics::poll_input_events()
{
    m_pimpl->poll_input_events();
}

void TXP::Graphics::build_imgui_contents(std::vector<Render_view_size>& out_rend_view_sizes)
{
    m_pimpl->build_imgui_contents(out_rend_view_sizes);
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

void TXP::Graphics::set_render_object_per_instance_data(
    std::vector<Render_object> const& rend_obj_list)
{
    m_pimpl->set_render_object_per_instance_data(rend_obj_list);
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

void TXP::Graphics::render_hdr_to_ldr_postprocessing()
{
    // @TEMPORARY: this is only for the main image, and this is simply to get the hdr image into the
    //             background of swapchain.
    // @TODO: only do this if no imgui. If yes imgui, have this be tonemapped into ldr and then get
    //        into an imgui image.
    auto const& swapchain_extent{ m_pimpl->gfx.swapchain_extent };
    m_pimpl->blit_image(m_pimpl->render_views[0].color_image.get_image(),
                        m_pimpl->render_views[0].color_image.get_extent(),
                        m_pimpl->gfx.swapchain_images[m_pimpl->current_swapchain_image_idx],
                        VkExtent3D{ .width = swapchain_extent.width,
                                    .height = swapchain_extent.height,
                                    .depth = 1 });
}

void TXP::Graphics::render_imgui()
{
    m_pimpl->render_imgui();
}

void TXP::Graphics::start_next_frame()
{
    m_pimpl->start_next_frame();
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
