#if TXP_GFX_BACKEND_VULKAN

#include "gfx.h"

#include "gfx_vulkan_impl.h"

// vv Must be in this order vv
// clang-format off
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
// clang-format on

#include "btlogger.h"
#include "gfx_vulkan/vk_image.h"
#include "render_object/render_model.h"

#include <cassert>
#include <cerrno>
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

    // @TODO: @THEA: put these into their own shader render nodes.
    m_pimpl->init_vulkan_create_descriptors();
    m_pimpl->init_vulkan_create_pipelines();
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

void TXP::Graphics::load_material_assets(
    std::vector<Material_asset_create_info>&& material_assets,
    std::vector<Material_set_asset_create_info>&& material_set_assets)
{   // Load materials.
    for (auto const& mat_asset : material_assets)
    {

    }
    BT_TRACEF("Loaded all %zu materials.", material_assets.size());

    // Load material sets.
    for (auto const& mat_set_asset : material_set_assets)
    {

    }
    BT_TRACEF("Loaded all %zu material sets.", material_set_assets.size());
}

void TXP::Graphics::load_model_assets(std::vector<Model_asset_create_info>&& model_assets,
                                      Render_model_data_collection& render_model_data_collection)
{   // Load models.
    for (auto const& mod_asset : model_assets)
    {
        load_model_from_disk(render_model_data_collection,
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

void TXP::Graphics::build_imgui_contents()
{
    m_pimpl->build_imgui_contents();
}

void TXP::Graphics::start_new_frame()
{
    m_pimpl->start_new_frame();
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

void TXP::Graphics::render_opaque_geometry()
{
    m_pimpl->clear_image(m_pimpl->hdr_draw_image_color.get_image(),
                         m_pimpl->hdr_draw_image_depth.get_image());
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
    auto const& swapchain_extent{ m_pimpl->gfx.swapchain_extent };
    m_pimpl->blit_image(m_pimpl->hdr_draw_image_color.get_image(),
                        m_pimpl->hdr_draw_image_color.get_extent(),
                        m_pimpl->gfx.swapchain_images[m_pimpl->current_swapchain_image_idx],
                        VkExtent3D{ .width = swapchain_extent.width,
                                    .height = swapchain_extent.height,
                                    .depth = 1 });
}

void TXP::Graphics::render_imgui()
{
    m_pimpl->render_imgui();
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
