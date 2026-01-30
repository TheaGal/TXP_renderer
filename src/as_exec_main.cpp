#include "txp_renderer_public.h"

#include <iostream>
#include <cstdint>

#warning This file should only be compiled inside the demo-test version of the application.


int32_t main()
{
    std::cout << "Hello warld!" << std::endl;

    TXP::Renderer r{ "My renderer test!" };

    

    return 0;
}
