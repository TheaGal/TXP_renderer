#include "txp_renderer_public.h"
#define BT_SERVICE_FINDER_IMPLEMENTATION
#include "btservice_finder.h"

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
    // r.add_material("default_mat",  // @TODO: implement this later!!!
    //                { "default_shader", TXP::Shader_Creation::SHAD_PIPE_TYPE_VERTEX_FRAGMENT },
    //                { { "texture0", "default_tex" } });
    r.add_material("__gradient_mat",
                   { "gradient", TXP::Shader_Creation::SHAD_PIPE_TYPE_COMPUTE },
                   { { "image", "__hdr_draw_image_color" } });
    #if 0  // @TODO: implement later!!!!
    r.add_material_set("default_mat_set", { "default_mat" });
    #endif // 0  // @TODO: implement later!!!!
    r.add_model("probuilder_example", ".wobj");
    r.add_model("simple_combat_char", ".glb");

    auto ro0_key = r.create_render_obj({
        .layer      = TXP::RENDER_LAYER_DEFAULT,
        .model_name = "default_model",
    });

    r.run();

    return 0;
}
