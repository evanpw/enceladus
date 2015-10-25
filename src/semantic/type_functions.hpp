#ifndef TYPE_FUNCTIONS_HPP
#define TYPE_FUNCTIONS_HPP

#include "semantic/types.hpp"
#include <unordered_map>

Type* instantiate(Type* type, TypeAssignment& replacements);
Type* instantiate(Type* type);
Trait* instantiate(Trait* trait);

Type* substitute(Type* original, const TypeAssignment& typeAssignment);

bool hasOverlappingInstance(Trait* trait, Type* type);

std::pair<bool, std::string> bindVariable(Type* variable, Type* value);
std::pair<bool, std::string> tryUnify(Type* lhs, Type* rhs);
bool occurs(const TypeVariable* variable, Type* value);

bool equals(Type* lhs, Type* rhs);

bool overlap(Type* lhs, Type* rhs);

#endif
