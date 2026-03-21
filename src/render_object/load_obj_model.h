#pragma once

#include "render_object/render_object.h"

#include <string>


namespace TXP
{

struct Material_organizer;  // Forward decl.

void load_obj_model_from_disk(Render_model_data_collection& data_collection,
                              Material_organizer& material_organizer,
                              std::string const& model_name,
                              std::string const& fname);

}  // namespace TXP
