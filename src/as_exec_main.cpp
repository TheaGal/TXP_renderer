#include "entt/entity/registry.hpp"
#include "input_handler/input_handler.h"  // @TODO: put in public folder.
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
        auto& rend_obj_cfg = ecs_registry.emplace<TXP::Render_object_config>(ecs_entity);

        rend_obj_cfg.layer = TXP::RENDER_LAYER_DEFAULT;
        rend_obj_cfg.model_name = "probuilder_example";
    }

    TXP::Input::Input_handler input_handler;

    TXP::Renderer r{ ecs_registry,
                     "My renderer test!",
                     1280,
                     720,
                     "assets/textures/",
                     "assets/shaders/",
                     "assets/models/" };

    r.add_texture("default_tex", ".ktx2");
    r.add_material("default_mat", "basic_diffuse", { { "texture0", "default_tex" } });
    r.add_material("__gradient_mat", "gradient", { { "image", "__hdr_draw_image_color" } });
    r.add_material_set("default_material_palette", { "default_mat" });
    r.add_model("probuilder_example", ".wobj");
    r.add_model("simple_combat_char", ".glb");

    // @TODO: this may not be wanted way of doing this (see macro POSSIBLY_REMOVE_THIS_LETS_SEE)
    // auto ro0_key = r.create_render_obj({
    //     .layer      = TXP::RENDER_LAYER_DEFAULT,
    //     .model_name = "default_model",
    // });

    r.run();

    return 0;
}
