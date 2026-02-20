#include "btuuid.h"

#include <random>


namespace
{

std::array<int, std::mt19937::state_size> generate_seed_data()
{
    std::array<int, std::mt19937::state_size> seed_data{};
    std::random_device rd;
    std::generate(std::begin(seed_data), std::end(seed_data), std::ref(rd));
    return seed_data;
}

}  // namespace


BT::UUID BT::UUID_helper::generate_uuid()
{
    static thread_local auto s_seed_data{ generate_seed_data() };
    static thread_local std::seed_seq s_seq(std::begin(s_seed_data), std::end(s_seed_data));
    static thread_local std::mt19937 s_generator(s_seq);
    static thread_local uuids::uuid_random_generator s_gen{s_generator};

    UUID new_uuid{ s_gen() };
    assert(!new_uuid.is_nil());
    assert(new_uuid.as_bytes().size() == 16);
    assert(new_uuid.version() == uuids::uuid_version::random_number_based);
    assert(new_uuid.variant() == uuids::uuid_variant::rfc);

    return new_uuid;
}
