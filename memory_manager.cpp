#include <iostream>
#include "ast.hpp"
#include "memory_manager.hpp"

using namespace std;

list<AstNode*> MemoryManager::allNodes_;

void MemoryManager::addNode(AstNode* node)
{
	allNodes_.push_back(node);
}

void MemoryManager::freeNodes()
{
	for (list<AstNode*>::iterator i = allNodes_.begin();
		i != allNodes_.end();
		++i)
	{
		delete *i;
	}
	
	allNodes_.clear();
}
