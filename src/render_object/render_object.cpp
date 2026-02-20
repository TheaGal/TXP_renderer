#include "render_object.h"

#include "btglm.h"
#include "txp_renderer/types.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>


namespace TXP
{

struct Render_model_data_collection::Data
{

};

Render_model_data_collection::Render_model_data_collection()
    : data(std::make_unique<Data>())
{
}

Render_model_data_collection::~Render_model_data_collection() = default;


uint16_t Render_model_data_collection::emplace_static_model_data_set(std::string const& name,
                                                                     Static_model_data_set&& data)
{
    assert(false);
}

uint16_t Render_model_data_collection::get_static_model_data_set(std::string const& name)
{
    assert(false);
}


uint16_t Render_model_data_collection::emplace_deformed_model_skin(std::string const& name, Deformed_model_skin&& data)
{
    assert(false);
}

uint16_t Render_model_data_collection::get_deformed_model_skin(std::string const& name)
{
    assert(false);
}

uint16_t Render_model_data_collection::emplace_deformed_model_anim_set(
    std::string const& name,
    Deformed_model_animation_set&& data)
{
    assert(false);
}

uint16_t Render_model_data_collection::get_deformed_model_anim_set(std::string const& name)
{
    assert(false);
}

uint16_t Render_model_data_collection::emplace_deformed_vertex_buffer(std::string const& name, void* data)
{
    assert(false);
}

uint16_t Render_model_data_collection::get_deformed_vertex_buffer(std::string const& name)
{
    assert(false);
}


uint16_t Render_model_data_collection::emplace_material_set(std::string const& name, Material_set&& data)
{
    assert(false);
}

uint16_t Render_model_data_collection::get_material_set(std::string const& name)
{
    assert(false);
}

}  // namespace TXP
