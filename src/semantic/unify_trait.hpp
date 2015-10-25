#ifndef UNIFY_TRAIT_HPP
#define UNIFY_TRAIT_HPP

#include "semantic/types.hpp"
#include <string>
#include <utility>

std::pair<bool, std::string> tryUnify(const Type* type, const Trait* trait);

#endif