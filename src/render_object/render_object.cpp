#include "render_object.h"

#include "btdatecheck.h"
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
#include <unordered_map>


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

        /// Calculates next available index in pool.
        size_t get_next_available_idx() const
        {
            return name_to_idx_map.size();
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

    /// Reference counter.
    std::unordered_map<uint16_t, size_t> model_reference_count_map;

    /// Starting index for deformed models.
    uint16_t starting_deformed_model_idx{ (uint16_t)-1 };

    // Deformed model set.
    std::unordered_map<uint16_t, Deformed_model_data_set> deformed_model_idx_map;
};

Render_model_data_collection::Render_model_data_collection()
    : inner_data(std::make_unique<Data>())
{
}

Render_model_data_collection::~Render_model_data_collection() = default;


void Render_model_data_collection::emplace_static_model_data_set(std::string const& name,
                                                                 Static_model_data_set&& data)
{
    if (inner_data->starting_deformed_model_idx != (uint16_t)-1)
        throw std::runtime_error("Number of static models already locked in.");

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


void Render_model_data_collection::lock_in_number_of_static_models()
{
    if (inner_data->starting_deformed_model_idx != (uint16_t)-1)
        throw std::runtime_error("Number of static models already locked in.");

    inner_data->starting_deformed_model_idx =
        inner_data->static_model_data_set_pool.get_next_available_idx();
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

uint16_t Render_model_data_collection::create_deformed_model_from_static_model_data_set(
    uint16_t static_model_data_set_idx)
{
    if (inner_data->starting_deformed_model_idx == (uint16_t)-1)
        throw std::runtime_error("Number of static models not locked in yet.");

    // Find next available idx in deformed model indexes.
    auto valid_idx = inner_data->starting_deformed_model_idx;
    for (; valid_idx < (uint16_t)-1; valid_idx++)
    {
        if (inner_data->deformed_model_idx_map.find(valid_idx) ==
            inner_data->deformed_model_idx_map.end())
            break;  // Found valid index! Break!
    }
    if (valid_idx == (uint16_t)-1)
        throw std::runtime_error("No valid idx found.");

    // Create model.
    auto const& base_static_model{ get_static_model_data_set(static_model_data_set_idx) };
    Static_model_data_set partial_copy{
        .meshes = base_static_model.meshes,
        .vertices = {},  // Don't store vertices since these will be calculated in the gpu skinning.
        .model_aabb = base_static_model.model_aabb,
    };

    Deformed_model_data_set new_deformed_model{
        .base_static_model_idx = static_model_data_set_idx,
        .deformed_model = std::move(partial_copy),
    };

    inner_data->deformed_model_idx_map.emplace(valid_idx, std::move(new_deformed_model));

    return valid_idx;
}

uint16_t Render_model_data_collection::translate_to_static_model_data_set_idx(
    uint16_t render_model_idx) const
{
    if (inner_data->starting_deformed_model_idx == (uint16_t)-1)
        throw std::runtime_error("Number of static models not locked in yet.");

    // Ignore translation if already in static model idx range.
    if (render_model_idx < inner_data->starting_deformed_model_idx)
        return render_model_idx;

    return inner_data->deformed_model_idx_map.at(render_model_idx).base_static_model_idx;
}

std::vector<Deformed_model_data_set*> Render_model_data_collection::get_all_deformed_models()
{
    std::vector<uint16_t> jojo;  // @TEST: @DEBUG: @NOCHECKIN
    std::vector<Deformed_model_data_set*> deformed_refs;
    deformed_refs.reserve(inner_data->deformed_model_idx_map.size());

    for (auto& elem : inner_data->deformed_model_idx_map)
    {
        jojo.emplace_back(elem.first);
        deformed_refs.emplace_back(&elem.second);
    }
    jojo.emplace_back(6969);
    for (auto& elem : inner_data->deformed_model_idx_map)
    {
        jojo.emplace_back(elem.first);
    }
    BT::date_deadline(2026, 4, 10);  // @TODO: CHECK THAT THE `jojo` ORDER IS CONSISTENT!!

    return deformed_refs;
}

void Render_model_data_collection::report_one_user_added(uint16_t render_model_idx)
{
    inner_data->model_reference_count_map[render_model_idx]++;
}

void Render_model_data_collection::report_one_user_removed(uint16_t render_model_idx,
                                                           bool& out_now_unused)
{
    inner_data->model_reference_count_map[render_model_idx]--;

    out_now_unused = (inner_data->model_reference_count_map.at(render_model_idx) <= 0);
}

}  // namespace TXP
