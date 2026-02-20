#pragma once

// Uses https://github.com/mariusbancila/stduuid (MIT License)
#include "../../third_party/stduuid/include/uuid.h"
#include "btjson.h"

#include <cassert>
#include <string>

using std::string;


namespace BT
{

/// `UUID()` creates a null, invalid UUID.
using UUID = uuids::uuid;

namespace UUID_helper
{

/// Randomly generates a valid UUID.
UUID generate_uuid();

inline static string to_pretty_repr(UUID const& my_uuid)
{
    return uuids::to_string(my_uuid);
}

inline static UUID to_UUID(string const& pretty_uuid)
{
    auto opt_uuid{ uuids::uuid::from_string(pretty_uuid) };
    assert(opt_uuid.has_value());
    assert(uuids::to_string(opt_uuid.value()) == pretty_uuid);
    return opt_uuid.value();
}

}  // namespace UUID_helper
}  // namespace BT


// Namespace where actual, original `BT::UUID` comes from, so that json serialization functions are
// located in the correct namespace.
namespace uuids
{

// ---- UUID serialization -------------------------------------------------------------------------
template<
    typename BasicJsonType,
    nlohmann::detail::enable_if_t<nlohmann::detail::is_basic_json<BasicJsonType>::value, int> = 0>
void to_json(BasicJsonType& nlohmann_json_j, const BT::UUID& nlohmann_json_t)
{
    nlohmann_json_j = BT::UUID_helper::to_pretty_repr(nlohmann_json_t);
}

template<
    typename BasicJsonType,
    nlohmann::detail::enable_if_t<nlohmann::detail::is_basic_json<BasicJsonType>::value, int> = 0>
void from_json(const BasicJsonType& nlohmann_json_j, BT::UUID& nlohmann_json_t)
{
    if (!nlohmann_json_j.is_null() &&
        nlohmann_json_j.is_string())
    {   // Read UUID string.
        std::string uuid_str{ nlohmann_json_j.template get<std::string>() };
        nlohmann_json_t = BT::UUID_helper::to_UUID(uuid_str);
    }
    else
    {   // Give default nil UUID.
        nlohmann_json_t = BT::UUID();
    }
}
// -------------------------------------------------------------------------------------------------

}  // namespace uuids
