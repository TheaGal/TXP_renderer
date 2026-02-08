#include "txp_renderer_public.h"

#include <iostream>
#include <cstdint>

#warning This file should only be compiled inside the demo-test version of the application.


int32_t main()
{
    TXP::Renderer r{ "My renderer test!",
                     1280,
                     720,
                     "assets/textures/",
                     "assets/shaders/",
                     "assets/models/" };

    r.add_texture("default_tex", ".ktx2");
    r.add_material("default_mat", "default_shader", { { "texture0", "default_tex" } });
    r.add_material_set("default_mat_set", { "default_mat" });
    r.add_model("default_model", ".glb");

    auto ro0_key = r.create_render_obj({
        .layer      = TXP::RENDER_LAYER_DEFAULT,
        .model_name = "default_model",
    });

    r.run();

    return 0;
}
