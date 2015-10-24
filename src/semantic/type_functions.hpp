#ifndef TYPE_FUNCTIONS_HPP
#define TYPE_FUNCTIONS_HPP

#include "semantic/types.hpp"
#include <map>

Type* instantiate(Type* type, std::map<TypeVariable*, Type*>& replacements);
Type* instantiate(Type* type);
const Trait* instantiate(const Trait* trait);

bool hasOverlappingInstance(Trait* trait, Type* type);

std::pair<bool, std::string> bindVariable(Type* variable, const Type* value);
std::pair<bool, std::string> tryUnify(Type* lhs, Type* rhs);
bool occurs(const TypeVariable* variable, const Type* value);

bool equals(const Type* lhs, const Type* rhs);

bool overlap(const Type* lhs, const Type* rhs);

#endif
