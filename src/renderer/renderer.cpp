#include "txp_renderer/renderer.h"

#include "btservice_finder.h"
#include "gfx.h"
#include "render_object/render_model.h"
#include "shader/shader_basic_diffuse.h"
#include "shader/shader_gradient.h"
#include "types.h"

#include <atomic>
#include <cassert>
#include <iostream>
#include <stdexcept>


namespace TXP
{

Renderer::Renderer(std::string const& title,
                   int32_t width,
                   int32_t height,
                   std::string const& texture_asset_dir,
                   std::string const& shader_asset_dir,
                   std::string const& model_asset_dir)
    : m_title(title)
    , m_width(width)
    , m_height(height)
    , m_texture_asset_dir(texture_asset_dir)
    , m_shader_asset_dir(shader_asset_dir)
    , m_model_asset_dir(model_asset_dir)
{   // Ensure only one instance.
    static std::atomic_bool s_init{ false };
    bool expect_init{ false };
    if (!s_init.compare_exchange_strong(expect_init, true))
    {
        throw std::runtime_error("Only one `TXP::Renderer` may be created.");
    }

    // Small setup of auxiliary systems.
    Shader_Creation::set_shader_directory(m_shader_asset_dir);
    set_model_directory(m_model_asset_dir);

    // Add self as service.
    BT_SERVICE_FINDER_ADD_SERVICE(Renderer, this);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Render loop.
void Renderer::run()
{   // Setup renderer.
    Graphics g(m_title, m_width, m_height);

    m_asset_reg_window_open = false;

    // Load textures.
    g.load_texture_assets(m_texture_asset_dir, std::move(*m_texture_assets.scoped_lock()));

    // Create shaders.
    Shader::Shader_gradient shad_gradient{ g.get_impl() };
    Shader::Shader_basic_diffuse shad_basic_diffuse{ g.get_impl() };

    // Load materials (and material sets).
    g.load_material_assets(std::move(*m_material_assets.scoped_lock()),
                           std::move(*m_material_set_assets.scoped_lock()));

    // Load models.
    g.load_model_assets(std::move(*m_model_assets.scoped_lock()), m_render_model_data_collection);

    // Render frames until shutdown flag is tripped.
    while (!m_shutdown_flag.load())
    {   // Process render object destroy requests.

        // Process render object create requests.

        // Update animator timers (renderer-profile).

        // Calculate animator joints.

        // Poll for input events.
        g.poll_input_events();
        m_camera.update();

        // Build imgui for this frame.
        std::vector<Render_view_size> render_view_sizes;
        g.build_imgui_contents(render_view_sizes);

        // Wait until can start rendering.
        g.wait_until_can_start_next_frame();

        // Set render view sizes.
        g.set_render_view_sizes(render_view_sizes);
        m_camera.set_render_view_sizes(render_view_sizes);

        size_t render_view_idx{ 0 };
        for (auto const& cam_matrix : m_camera.calc_cam_matrices())
        {
            bool main_cam_matrix{ render_view_idx == 0 };

            auto render_frame{ g.start_new_frame(render_view_idx) };

            if (main_cam_matrix)
            {
                // g.compute_light_culling();
                // g.compute_shadow_culling();
                // g.compute_opaque_geometry_culling();
                // g.compute_transparent_geometry_culling();
            }

            // g.render_shadows(render_frame);
            shad_gradient.compute(render_frame);  // @TODO: this needs to get changed to image-type GENERAL before compute shader usage.
            shad_basic_diffuse.draw(render_frame);

            if (main_cam_matrix)
            {
                // g.render_clouds();
                // g.render_volumetric_light();
            }

            // g.render_particles();
            // g.render_transparent_geometry();

            render_view_idx++;
        }
        if (render_view_idx == 0)
        {
            throw std::runtime_error("Main render view must exist.");
        }

        g.render_hdr_to_ldr_postprocessing();
        g.render_imgui();

        g.present_frame_to_screen();

        std::cout << "RENDERED ONE FRAME" << std::endl;
    }

    // Wait until GPU is idle before destruction.
    g.wait_until_gpu_idle();
}

void Renderer::shutdown_loop()
{
    m_shutdown_flag.store(true);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Asset loading.
void Renderer::add_texture(std::string const& texture_name, std::string const& file_ext)
{
    if (!m_asset_reg_window_open.load())
        throw std::runtime_error("Cannot load assets once asset loading stage has started.");

    if (file_ext != ".ktx2")
        throw std::runtime_error("Only .ktx2 file type for textures are allowed.");

    m_texture_assets.scoped_lock()->emplace_back(texture_name, texture_name + ".ktx2");
}

void Renderer::add_material(
    std::string const& material_name,
    std::pair<std::string, Shader_Creation::Shader_pipeline_type>&& shader_name_and_type,
    std::unordered_map<std::string, std::string> const& shader_params)
{
    if (!m_asset_reg_window_open.load())
        throw std::runtime_error("Cannot load assets once asset loading stage has started.");

    m_material_assets.scoped_lock()->emplace_back(material_name,
                                                  shader_name_and_type,
                                                  shader_params);
}

void Renderer::add_material_set(std::string const& mat_set_name,
                                std::vector<std::string>&& materials)
{
    if (!m_asset_reg_window_open.load())
        throw std::runtime_error("Cannot load assets once asset loading stage has started.");

    m_material_set_assets.scoped_lock()->emplace_back(mat_set_name, std::move(materials));
}

void Renderer::add_model(std::string const& model_name,
                         std::string const& file_ext)
{
    if (!m_asset_reg_window_open.load())
        throw std::runtime_error("Cannot load assets once asset loading stage has started.");

    m_model_assets.scoped_lock()->emplace_back(model_name, file_ext);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Render object lifetime.
pool_key_t Renderer::create_render_obj(Render_obj_create_config&& config)
{
    return 0;
}

void Renderer::destroy_render_obj(pool_key_t key)
{}

}  // namespace TXP
