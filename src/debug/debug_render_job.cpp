#include "txp_renderer/debug/debug_render_job.h"

#include "btdatecheck.h"
#include "btglm.h"

#include <string>


namespace TXP
{

debug::debug_model_id_t debug::emplace_debug_model(std::string const& model_name,
                                                   Material_type material)
{
    BT::date_deadline(2026, 8, 25);
    return 0;
}

void debug::remove_debug_model(debug_model_id_t model_id)
{
    BT::date_deadline(2026, 8, 25);
}

void debug::update_debug_model_transform(debug_model_id_t model_id, mat4 transform)
{
    BT::date_deadline(2026, 8, 25);
}

void debug::emplace_debug_line(Debug_line&& line, float_t duration)
{
    BT::date_deadline(2026, 8, 25);
}

} // namespace TXP
