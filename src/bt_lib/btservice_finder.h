#pragma once

#include "btlogger.h"

#include <cassert>
#include <type_traits>
#include <typeindex>
#include <unordered_map>


namespace BT
{
namespace service_finder
{

namespace internal
{   // Map that holds all services.
    extern std::unordered_map<std::type_index, void*> s_services_map;
}

// Adds service with some checking of whether the service type is unique.
// (i.e. Runtime check that type ID is unique amongst all services and/or a service
//  isn't getting added twice.)
#define BT_SERVICE_FINDER_ADD_SERVICE(service_typename, service_ptr)                                \
    do {                                                                                            \
    static_assert(std::is_class<service_typename>::value, "Typename must be a class.");             \
    bool insertion_success{                                                                         \
        ::BT::service_finder::internal                                                              \
            ::s_services_map.emplace(std::type_index(typeid(service_typename)),                     \
                                     reinterpret_cast<void*>(service_ptr))                          \
            .second };                                                                              \
    BT::logger::printef(insertion_success ? BT::logger::TRACE : BT::logger::ERROR,                  \
                        "Adding service \"%s\": %s",                                                \
                        #service_typename,                                                          \
                        insertion_success ? "SUCCESS" : "FAILURE");                                 \
    assert(insertion_success);                                                                      \
    } while(false)

// Finds service that should have been added with the BT_SERVICE_FINDER_ADD_SERVICE
// macro. Throws if service is missing.
template<typename T>
T& find_service()
{
    static_assert(std::is_class<T>::value, "Typename must be a class.");
    auto service_ptr{ internal::s_services_map.at(std::type_index(typeid(T))) };
    assert(service_ptr != nullptr);
    return *reinterpret_cast<T*>(service_ptr);
}

}  // namespace service_finder
}  // namespace BT


// Use this macro in one compilation unit to implement the service map.
#ifdef BT_SERVICE_FINDER_IMPLEMENTATION

std::unordered_map<std::type_index, void*> BT::service_finder::internal::s_services_map;

#endif  // BT_SERVICE_FINDER_IMPLEMENTATION
