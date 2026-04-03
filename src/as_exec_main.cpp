#include "entt/entity/registry.hpp"
#include "txp_renderer_public.h"
#define BT_SERVICE_FINDER_IMPLEMENTATION
#include "btservice_finder.h"  // @TODO: put in public folder.

#include <cstdint>

#warning This file should only be compiled inside the demo-test version of the application.


int32_t main()
{
    entt::registry ecs_registry;
    {   // Create sample entity.
        auto ecs_entity = ecs_registry.create();
        auto& rend_obj_cfg = ecs_registry.emplace<TXP::component::Render_object_config>(ecs_entity);

        rend_obj_cfg.render_layer = TXP::RENDER_LAYER_DEFAULT;
        rend_obj_cfg.model_name = "probuilder_example";
    }
    {   // Create sample entity 2.
        auto ecs_entity = ecs_registry.create();
        auto& rend_obj_cfg = ecs_registry.emplace<TXP::component::Render_object_config>(ecs_entity);

        rend_obj_cfg.render_layer = TXP::RENDER_LAYER_DEFAULT;
        rend_obj_cfg.model_name = "probuilder_example";
        glm_scale_make(rend_obj_cfg.transform.raw, vec3{ 0.25f, 0.25f, 0.25f });
        glm_translate(rend_obj_cfg.transform.raw, vec3{ 100, 0, 0 });
    }
    {   // Create sample entity 3.
        auto ecs_entity = ecs_registry.create();
        auto& rend_obj_cfg = ecs_registry.emplace<TXP::component::Render_object_config>(ecs_entity);

        rend_obj_cfg.render_layer = TXP::RENDER_LAYER_DEFAULT;
        rend_obj_cfg.model_name = "simple_combat_char";
        glm_translate_make(rend_obj_cfg.transform.raw, vec3{ -25, 5, 6 });
        rend_obj_cfg.is_deformed = false;
    }
    {   // Create sample entity 4.
        auto ecs_entity = ecs_registry.create();
        auto& rend_obj_cfg = ecs_registry.emplace<TXP::component::Render_object_config>(ecs_entity);

        rend_obj_cfg.render_layer = TXP::RENDER_LAYER_DEFAULT;
        rend_obj_cfg.model_name = "simple_combat_char";
        glm_translate_make(rend_obj_cfg.transform.raw, vec3{ -25, 5, 0 });
        rend_obj_cfg.is_deformed = true;
    }

    TXP::Input::Input_handler input_handler;

    TXP::Renderer r{ ecs_registry,
                     "My renderer test!",
                     1280,
                     720,
                     "assets/textures/",
                     "assets/shaders/",
                     "assets/models/",
                     "assets/anim_frame_actions/",
                     [](bool) { },
                     []() { return false; } };

    r.add_texture("default_tex", ".ktx2");
    r.add_material("default_mat", "basic_diffuse", { { "texture0", "default_tex" } });
    r.add_material("ProBuilderDefault", "basic_diffuse", { { "texture0", "default_tex" } });
    r.add_material("__gradient_mat", "gradient", { { "image", "__hdr_draw_image_color" } });
    r.add_material_palette("default_material_palette", { "default_mat" });
    r.add_model("probuilder_example", ".wobj", false, false);
    r.add_model("simple_combat_char", ".glb", true, true);
    r.build();

    // @TODO: this may not be wanted way of doing this (see macro POSSIBLY_REMOVE_THIS_LETS_SEE)
    // auto ro0_key = r.create_render_obj({
    //     .layer      = TXP::RENDER_LAYER_DEFAULT,
    //     .model_name = "default_model",
    // });

    while (!r.is_requesting_shutdown())
    {
        r.poll_input_events();
        r.render_one_frame(1.0f / 60.0f);  // @HARDCODE
    }

    return 0;
}
