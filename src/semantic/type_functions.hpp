#ifndef TYPE_FUNCTIONS_HPP
#define TYPE_FUNCTIONS_HPP

#include "semantic/types.hpp"
#include <map>

Type* instantiate(Type* type, std::map<TypeVariable*, Type*>& replacements);
Type* instantiate(Type* type);

bool hasOverlappingInstance(Trait* trait, Type* type);

std::pair<bool, std::string> bindVariable(Type* variable, Type* value);
std::pair<bool, std::string> tryUnify(Type* lhs, Type* rhs);
bool occurs(TypeVariable* variable, Type* value);

bool isSubtype(const Type* lhs, const Trait* trait);
bool isSubtype(const Type* lhs, const Type* rhs);

bool overlap(const Type* lhs, const Type* rhs);

#endif
