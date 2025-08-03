#ifndef __WBE_RESOURCE_UNIT_H__
#define __WBE_RESOURCE_UNIT_H__

#include <type_traits>
namespace WhiteBirdEngine {

struct ResourceUnit {};

template <typename T>
constexpr bool is_resource_unit = std::is_base_of_v<ResourceUnit, std::remove_cvref_t<T>>;

template <typename T>
concept ResourceUnitConcept = is_resource_unit<T>;

}

#endif
