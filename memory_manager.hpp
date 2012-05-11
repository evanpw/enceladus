#ifndef MEMORY_MANAGER_HPP
#define MEMORY_MANAGER_HPP

#include <list>

class AstNode;

/*
 * Keeps track of all allocated AST nodes, so that they can all be freed at the
 * end of compilation.
 */
class MemoryManager
{
public:
	static void addNode(AstNode* node);
	static void freeNodes();
	
private:
	static std::list<AstNode*> allNodes_;
};

#endif
