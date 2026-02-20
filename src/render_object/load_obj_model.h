#pragma once

#include "render_object/render_object.h"

#include <string>


namespace TXP
{

void load_obj_model_from_disk(Render_model_data_collection& data_collection,
                              std::string const& model_name,
                              std::string const& fname);

}  // namespace TXP
