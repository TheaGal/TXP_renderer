#include "txp_renderer_public.h"

#include <iostream>
#include <cstdint>

#warning This file should only be compiled inside the demo-test version of the application.


int32_t main()
{
    std::cout << "Hello warld!" << std::endl;

    TXP::Renderer r{ "My renderer test!", 1024, 576 };

    auto ro0_key = r.create_render_obj({
        .layer      = TXP::RENDER_LAYER_DEFAULT,
        .model_name = "DefaultModel",
    });

    r.run();

    return 0;
}
