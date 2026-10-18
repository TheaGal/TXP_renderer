#pragma once

#include "txp_renderer/ui/ui_state.h"

#include <vector>


namespace TXP
{

struct UI_canvas_state::UI_element_list_container
{
    std::vector<UI::UI_element> elems;
};

} // namespace TXP
