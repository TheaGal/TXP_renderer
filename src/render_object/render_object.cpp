#include "render_object.h"

#include "btglm.h"
#include "render_object/deformed_render_model.h"
#include "render_object/render_model.h"
#include "render_object/skeletal_animation.h"
#include "txp_renderer/types.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <stdexcept>


namespace TXP
{

struct Render_model_data_collection::Data
{
    template<typename T>
    class Data_pool
    {
    public:
        static constexpr size_t k_array_pool_size{ 0xFFFF };  // 2^16 - 1 (so -1 is reserved).

        /// Emplaces an element into the pool, ensuring the pool doesn't increase in size past
        /// `k_array_pool_size`.
        void emplace(std::string const& name, T&& data)
        {
            if (name_to_idx_map.find(name) != name_to_idx_map.end())
                throw std::runtime_error("Found duplicate key in data collection already.  name=" +
                                         name);

            // Check size isn't overflown.
            size_t next_ins_pos{ name_to_idx_map.size() };
            if (next_ins_pos >= k_array_pool_size)
                throw std::runtime_error("Attempting to emplace into a full data collection.");

            // Perform emplace.
            pool[next_ins_pos] = std::move(data);
            name_to_idx_map.emplace(name, static_cast<uint16_t>(next_ins_pos));
        }

        /// Gathers name list.
        std::vector<std::string> get_name_list() const
        {
            std::vector<std::string> names;
            names.reserve(name_to_idx_map.size());

            for (auto const& [name, _] : name_to_idx_map)
                names.emplace_back(name);

            return names;
        }

        /// Gets index of element from the name key.
        uint16_t get_idx(std::string const& name) const
        {
            return name_to_idx_map.at(name);
        }

        /// Gets element from index.
        T const& get(uint16_t idx) const
        {
            if (idx >= name_to_idx_map.size())
                throw std::runtime_error("Out of bounds index: " + std::to_string(idx));

            return pool[idx];
        }

    private:
        std::array<T, k_array_pool_size> pool;
        std::map<std::string, uint16_t> name_to_idx_map;
    };

    // Pools.
    Data_pool<Static_model_data_set> static_model_data_set_pool;
    Data_pool<Deformed_model_skin> deformed_model_skin_pool;
    Data_pool<Deformed_model_animation_set> deformed_model_anim_set_pool;
};

Render_model_data_collection::Render_model_data_collection()
    : inner_data(std::make_unique<Data>())
{
}

Render_model_data_collection::~Render_model_data_collection() = default;


void Render_model_data_collection::emplace_static_model_data_set(std::string const& name,
                                                                 Static_model_data_set&& data)
{
    inner_data->static_model_data_set_pool.emplace(name, std::move(data));
}

std::vector<std::string> Render_model_data_collection::get_static_model_data_set_name_list() const
{
    return inner_data->static_model_data_set_pool.get_name_list();
}

uint16_t Render_model_data_collection::get_static_model_data_set_idx(std::string const& name) const
{
    return inner_data->static_model_data_set_pool.get_idx(name);
}

Static_model_data_set const& Render_model_data_collection::get_static_model_data_set(
    uint16_t idx) const
{
    return inner_data->static_model_data_set_pool.get(idx);
}


void Render_model_data_collection::emplace_deformed_model_skin(std::string const& name,
                                                                   Deformed_model_skin&& data)
{
    inner_data->deformed_model_skin_pool.emplace(name, std::move(data));
}

std::vector<std::string> Render_model_data_collection::get_deformed_model_skin_name_list() const
{
    return inner_data->deformed_model_skin_pool.get_name_list();
}

uint16_t Render_model_data_collection::get_deformed_model_skin_idx(std::string const& name) const
{
    return inner_data->deformed_model_skin_pool.get_idx(name);
}

Deformed_model_skin const& Render_model_data_collection::get_deformed_model_skin(uint16_t idx) const
{
    return inner_data->deformed_model_skin_pool.get(idx);
}

void Render_model_data_collection::emplace_deformed_model_anim_set(
    std::string const& name,
    Deformed_model_animation_set&& data)
{
    inner_data->deformed_model_anim_set_pool.emplace(name, std::move(data));
}

std::vector<std::string> Render_model_data_collection::get_deformed_model_anim_set_name_list() const
{
    return inner_data->deformed_model_anim_set_pool.get_name_list();
}

uint16_t Render_model_data_collection::get_deformed_model_anim_set_idx(
    std::string const& name) const
{
    return inner_data->deformed_model_anim_set_pool.get_idx(name);
}

Deformed_model_animation_set const& Render_model_data_collection::get_deformed_model_anim_set(
    uint16_t idx) const
{
    return inner_data->deformed_model_anim_set_pool.get(idx);
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
