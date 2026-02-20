#include "render_model.h"

#include "btglm.h"
#include "load_gltf_model.h"
#include "load_obj_model.h"

#include <cassert>
#include <limits>
#include <stdexcept>


namespace
{

static std::string s_model_directory;

}  // namespace


void TXP::set_model_directory(std::string const& dir_path)
{
    s_model_directory = dir_path;
}


// AA_bounding_box
void TXP::AA_bounding_box::reset()
{
    min[0] = min[1] = min[2] = std::numeric_limits<float_t>::max();
    max[0] = max[1] = max[2] = std::numeric_limits<float_t>::lowest();
}

void TXP::AA_bounding_box::feed_position(vec3 position)
{
    glm_vec3_minv(min, position, min);
    glm_vec3_maxv(max, position, max);
}


TXP::Render_model TXP::load_model_from_disk(std::string const& model_name,
                                            std::string const& file_ext)
{
    if (file_ext == ".wobj")
    {
        return load_obj_model_from_disk(s_model_directory + model_name + file_ext);
    }
    else if (file_ext == ".glb" || file_ext == ".gltf")
    {
        return load_gltf_model_from_disk(s_model_directory + model_name + file_ext);
    }
    else
    {
        throw std::runtime_error("Unknown model asset file extension: " + file_ext);
    }
}
