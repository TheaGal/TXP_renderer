#include "btuuid.h"
#define BT_SERVICE_FINDER_IMPLEMENTATION
#include "btservice_finder.h"  // @TODO: put in public folder.
#include "entt/entity/registry.hpp"
#include "txp_renderer_public.h"

#include <cstdint>

#warning This file should only be compiled inside the demo-test version of the application.


int32_t main()
{
    entt::registry ecs_registry;
    {   // Create sample entity.
        auto ecs_entity = ecs_registry.create();

        auto& meta = ecs_registry.emplace<TXP::component::Entity_metadata>(ecs_entity);
        meta.uuid = BT::UUID_helper::generate_uuid();

        auto& rend_obj_cfg = ecs_registry.emplace<TXP::component::Render_object_config>(ecs_entity);
        rend_obj_cfg.render_layer = TXP::RENDER_LAYER_DEFAULT;
        rend_obj_cfg.model_name = "probuilder_example";
    }
    {   // Create sample entity 2.
        auto ecs_entity = ecs_registry.create();

        auto& meta = ecs_registry.emplace<TXP::component::Entity_metadata>(ecs_entity);
        meta.uuid = BT::UUID_helper::generate_uuid();

        auto& rend_obj_cfg = ecs_registry.emplace<TXP::component::Render_object_config>(ecs_entity);
        rend_obj_cfg.render_layer = TXP::RENDER_LAYER_DEFAULT;
        rend_obj_cfg.model_name = "probuilder_example";
        glm_scale_make(rend_obj_cfg.transform.raw, vec3{ 0.25f, 0.25f, 0.25f });
        glm_translate(rend_obj_cfg.transform.raw, vec3{ 100, 0, 0 });
    }
    {   // Create sample entity 3.
        auto ecs_entity = ecs_registry.create();

        auto& meta = ecs_registry.emplace<TXP::component::Entity_metadata>(ecs_entity);
        meta.uuid = BT::UUID_helper::generate_uuid();

        auto& rend_obj_cfg = ecs_registry.emplace<TXP::component::Render_object_config>(ecs_entity);
        rend_obj_cfg.render_layer = TXP::RENDER_LAYER_DEFAULT;
        rend_obj_cfg.model_name = "simple_combat_char";
        glm_translate_make(rend_obj_cfg.transform.raw, vec3{ -25, 5, 6 });
        rend_obj_cfg.is_deformed = false;
    }
    {   // Create sample entity 4.
        auto ecs_entity = ecs_registry.create();

        auto& meta = ecs_registry.emplace<TXP::component::Entity_metadata>(ecs_entity);
        meta.uuid = BT::UUID_helper::generate_uuid();

        auto& rend_obj_cfg = ecs_registry.emplace<TXP::component::Render_object_config>(ecs_entity);
        rend_obj_cfg.render_layer = TXP::RENDER_LAYER_DEFAULT;
        rend_obj_cfg.model_name = "simple_combat_char";
        glm_translate_make(rend_obj_cfg.transform.raw, vec3{ -25, 5, 0 });
        rend_obj_cfg.is_deformed = true;
    }
    {   // Create sample entity 5.
        auto ecs_entity = ecs_registry.create();

        auto& meta = ecs_registry.emplace<TXP::component::Entity_metadata>(ecs_entity);
        meta.uuid = BT::UUID_helper::generate_uuid();

        auto& rend_obj_cfg = ecs_registry.emplace<TXP::component::Render_object_config>(ecs_entity);
        rend_obj_cfg.render_layer = TXP::RENDER_LAYER_DEFAULT;
        rend_obj_cfg.model_name = "rails";
        rend_obj_cfg.sub_mesh_name = "CurveRail.L.001";
        glm_translate_make(rend_obj_cfg.transform.raw, vec3{ 0, 10, 0 });
        rend_obj_cfg.is_deformed = false;
    }

    TXP::Input::Input_handler input_handler;

    TXP::Renderer r{ ecs_registry,
                     "My renderer test!",
                     "assets/textures/",
                     "assets/shaders/",
                     "assets/models/",
                     "assets/anim_frame_actions/",
                     "assets/animator_templates/",
                     [](bool) {},
                     []() { return false; },
                     [](bool) {} };

    r.add_texture("default_tex", ".ktx2");
    r.add_material("default_mat", "basic_diffuse", { { "texture0", "default_tex" } });
    r.add_material("ProBuilderDefault", "basic_diffuse", { { "texture0", "default_tex" } });
    r.add_material("__gradient_mat", "gradient", { { "image", "__hdr_draw_image_color" } });
    r.add_material_palette("default_material_palette", { "default_mat" });
    r.add_model("probuilder_example", ".wobj", false, false);
    r.add_model("simple_combat_char", ".glb", true, true);
    r.add_model("rails", ".wobj", false, false);
    r.build();

    r.set_imgui_build_contents_callback([]() {});

    // @TODO: this may not be wanted way of doing this (see macro POSSIBLY_REMOVE_THIS_LETS_SEE)
    // auto ro0_key = r.create_render_obj({
    //     .layer      = TXP::RENDER_LAYER_DEFAULT,
    //     .model_name = "default_model",
    // });

    while (!r.is_requesting_shutdown())
    {
        r.poll_input_events();
        r.render_one_frame(1.0f / 60.0f);  // @HARDCODE

        r.report_performance_time(TXP::PERF_TIME_TYPE_SIMULATION_LOOP, 0.123);
        r.report_performance_time(TXP::PERF_TIME_TYPE_RENDERER_LOOP, 0.456);
    }

    return 0;
}
