#include "asm_printer.hpp"
#include "ast.hpp"
#include "ast_context.hpp"
#include "context.hpp"
#include "exceptions.hpp"
#include "machine_codegen.hpp"
#include "parser.hpp"
#include "reg_alloc.hpp"
#include "scope.hpp"
#include "semantic.hpp"
#include "tac_codegen.hpp"
#include "tac_validator.hpp"

#include <cstdio>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <stack>
#include <unordered_map>

using namespace std;

extern ProgramNode* parse();
extern FILE* yyin;

ProgramNode* root;

FILE* mainFile;
bool lastFile = false;

extern int yylineno;
extern int yycolumn;

extern "C" int yywrap()
{
	if (lastFile)
	{
		return 1;
	}
	else
	{
		yyin = mainFile;

		lastFile = true;
		yylineno = 1;
		yycolumn = 0;

		return 0;
	}
}

void dumpFunction(const Function* function)
{
	std::cerr << "function " << function->name << ":" << std::endl;

	for (BasicBlock* block : function->blocks)
	{
		std::cerr << block->str() << ":" << std::endl;
		Instruction* inst = block->first;
		while (inst != nullptr)
		{
			std::cerr << "\t" << inst->str() << std::endl;
			inst = inst->next;
		}
	}

	if (!function->locals.empty())
	{
		std::cerr << "locals:" << std::endl;
		for (Value* value : function->locals)
		{
			std::cerr << "\t" << value->str() << ":" << std::endl;
			assert(!value->definition);

			for (Instruction* inst : value->uses)
			{
				if (dynamic_cast<LoadInst*>(inst))
				{
					std::cerr << "\t\t(load) " << inst->str() << std::endl;
				}
				else if (dynamic_cast<StoreInst*>(inst))
				{
					std::cerr << "\t\t(store) " << inst->str() << std::endl;
				}
				else
				{
					assert(false);
				}
			}
		}
	}

	if (!function->temps.empty())
	{
		std::cerr << "temps:" << std::endl;
		for (Value* value : function->temps)
		{
			std::cerr << "\t" << value->str() << ":" << std::endl;

			assert(value->definition);
			std::cerr << "\t\t(defn) " << value->definition->str() << std::endl;

			for (Instruction* inst : value->uses)
			{
				std::cerr << "\t\t(use) " << inst->str() << std::endl;
			}
		}
	}

	std::cerr << "blocks:" << std::endl;
	for (BasicBlock* block : function->blocks)
	{
		std::cerr << "\t" << block->str() << " -> ";

		auto successors = block->successors();

		if (!successors.empty())
		{
			for (size_t i = 0; i < successors.size(); ++i)
			{
				if (i != 0)
					std::cerr << ", ";

				std::cerr << "(" << successors[i]->str() << ")";
			}
			std::cerr << std::endl;
		}
		else
		{
			std::cerr << "(no successors)" << std::endl;
		}
	}

	std::cerr << std::endl << std::endl;
}

ostream& operator<<(ostream& out, const std::set<BasicBlock*> blocks)
{
	out << "{";
	for (BasicBlock* block : blocks)
	{
		out << block->seqNumber << ",";
	}
	out << "}";

	return out;
}

// Got a lot of stuff from here: http://www.cs.utexas.edu/users/mckinley/380C/

struct PhiDescription
{
	PhiDescription(Value* original)
	: original(original)
	{}

	Value* original;

	Value* dest = nullptr;
	std::vector<std::pair<BasicBlock*, Value*>> sources;
};

typedef std::unordered_map<BasicBlock*, std::set<BasicBlock*>> dom_t;
typedef std::unordered_map<BasicBlock*, BasicBlock*> idom_t;
typedef std::unordered_map<BasicBlock*, std::vector<BasicBlock*>> df_t;
typedef std::unordered_map<BasicBlock*, std::vector<PhiDescription>> phis_t;

// Compute the dominators of each basic block
dom_t findDominators(const Function* function)
{
	dom_t dom;
	std::vector<BasicBlock*> blocks = function->blocks;
	BasicBlock* entry = blocks[0];

	// Only dominator of the entry block is itself
	dom[entry] = {entry};

	// The dominators of the rest of the blocks are a subset of the whole set
	for (size_t i = 1; i < blocks.size(); ++i)
	{
		dom[blocks[i]].insert(blocks.begin(), blocks.end());
	}

	// Simple N^2 algorithm based on the recursive definition of DOM
	bool changed;
	do
	{
		changed = false;

		for (size_t i = 1; i < blocks.size(); ++i)
		{
			BasicBlock* block = blocks[i];
			auto predecessors = block->predecessors();

			std::set<BasicBlock*> oldDom = dom[block];

			// The set of strict dominators of block is equal to the intersection
			// of the set of dominators of each of its predecessors
			std::set<BasicBlock*> newDom;
			if (!predecessors.empty())
			{
				newDom = dom[predecessors[0]];

				for (size_t i = 1; i < predecessors.size(); ++i)
				{
					std::set<BasicBlock*> lhs = newDom;
					const std::set<BasicBlock*>& rhs = dom[predecessors[i]];

					newDom.clear();
					std::set_intersection(
						lhs.begin(), lhs.end(),
						rhs.begin(), rhs.end(),
						std::inserter(newDom, newDom.begin()));
				}
			}

			newDom.insert(block);

			if (oldDom != newDom)
			{
				changed = true;
				dom[block] = newDom;
			}
		}

	} while (changed);

	return dom;
}

idom_t getImmediateDominators(const dom_t& dom)
{
	idom_t idom;

	for (auto& item : dom)
	{
		BasicBlock* block = item.first;
		const std::set<BasicBlock*>& dominators = item.second;

		// A copy
		std::set<BasicBlock*> working = item.second;

		// IDOM must be a strict dominator
		working.erase(block);

		// Remove all elements which are strictly dominated by a dominator
		for (BasicBlock* d1 : dominators)
		{
			if (d1 == block) continue;

			const std::set<BasicBlock*>& dominators1 = dom.at(d1);

			for (BasicBlock* d2 : dominators)
			{
				if (d1 != d2 && dominators1.find(d2) != dominators1.end())
				{
					working.erase(d2);
				}
			}
		}

		if (working.size() == 1)
		{
			idom[block] = *(working.begin());
		}
		else if (working.size() == 0)
		{
			// Should only happen for the entry block
			assert(block->predecessors().empty());
			idom[block] = nullptr;
		}
		else
		{
			assert(false);
		}
	}

	return idom;
}

df_t getDominanceFrontiers(const idom_t& idom)
{
	df_t df;

	for (auto& item : idom)
	{
		BasicBlock* block = item.first;
		BasicBlock* dominator = item.second;

		if (block->predecessors().size() < 2)
			continue;

		for (BasicBlock* predecessor : block->predecessors())
		{
			BasicBlock* runner = predecessor;
			while (runner != dominator)
			{
				df[runner].emplace_back(block);
				runner = idom.at(runner);
			}
		}
	}

	return df;
}

phis_t calculatePhiNodes(const Function* function, const df_t& df)
{
	phis_t result;

	for (Value* local : function->locals)
	{
		std::deque<BasicBlock*> workList;
		std::set<BasicBlock*> everOnWorkList;
		std::set<BasicBlock*> alreadyInserted;

		// Gather all blocks containing an assignment to this variable
		for (Instruction* inst : local->uses)
		{
			if (dynamic_cast<StoreInst*>(inst))
			{
				everOnWorkList.insert(inst->parent);
				workList.push_back(inst->parent);
			}
		}

		while (!workList.empty())
		{
			BasicBlock* next = workList.front();
			workList.pop_front();

			if (df.find(next) == df.end())
				continue;

			// Loop over the dominance frontier of this block
			for (BasicBlock* v : df.at(next))
			{
				if (alreadyInserted.find(v) != alreadyInserted.end())
					continue;

				result[v].emplace_back(local);
				alreadyInserted.insert(v);

				if (everOnWorkList.find(v) == everOnWorkList.end())
				{
					everOnWorkList.insert(v);
					workList.push_back(v);
				}
			}
		}
	}

	for (Value* param : function->params)
	{
		std::deque<BasicBlock*> workList;
		std::set<BasicBlock*> everOnWorkList;
		std::set<BasicBlock*> alreadyInserted;

		// Gather all blocks containing an assignment to this variable
		for (Instruction* inst : param->uses)
		{
			if (dynamic_cast<StoreInst*>(inst))
			{
				everOnWorkList.insert(inst->parent);
				workList.push_back(inst->parent);
			}
		}

		// Function arguments have an ssumed assignment in the entry node
		BasicBlock* entry = function->blocks[0];
		if (everOnWorkList.find(entry) == everOnWorkList.end())
		{
			everOnWorkList.insert(entry);
			workList.push_back(entry);
		}

		while (!workList.empty())
		{
			BasicBlock* next = workList.front();
			workList.pop_front();

			if (df.find(next) == df.end())
				continue;

			// Loop over the dominance frontier of this block
			for (BasicBlock* v : df.at(next))
			{
				if (alreadyInserted.find(v) != alreadyInserted.end())
					continue;

				result[v].emplace_back(param);
				alreadyInserted.insert(v);

				if (everOnWorkList.find(v) == everOnWorkList.end())
				{
					everOnWorkList.insert(v);
					workList.push_back(v);
				}
			}
		}
	}

	return result;
}

std::unordered_map<Value*, std::stack<Value*>> phiStack;
std::unordered_set<BasicBlock*> visited;
std::unordered_map<Value*, int> counter;

Value* generateName(Function* function, Value* variable)
{
	int i = counter[variable]++;
	std::stringstream ss;
	ss << variable->name << "." << i;

	Value* newName = function->makeTemp(ss.str());
	phiStack[variable].push(newName);

	return newName;
}

void rename(Function* function, BasicBlock* block, phis_t& phis)
{
	if (visited.find(block) == visited.end())
	{
		visited.insert(block);
	}
	else
	{
		return;
	}

	// Keep track of the names we've created so that we can easily undo it
	std::vector<Value*> toPop;

	// Rename the LHS of phi nodes
	if (phis.find(block) != phis.end())
	{
		for (PhiDescription& phiDesc : phis.at(block))
		{
			phiDesc.dest = generateName(function, phiDesc.original);
			toPop.push_back(phiDesc.original);
		}
	}

	// Rewrite load and store instructions with new names
	for (Instruction* inst = block->first; inst != nullptr;)
	{
		if (LoadInst* load = dynamic_cast<LoadInst*>(inst))
		{
			if (phiStack[load->src].empty())
			{
				// A load without a previous store should only be possible for
				// a function parameter
				assert(dynamic_cast<Argument*>(load->src));

				// For any node dominated by this one, don't re-load, just
				// use this value
				phiStack[load->src].push(load->dest);
				toPop.push_back(load->src);
			}
			else
			{
				Value* newName = phiStack[load->src].top();

				inst = inst->next;
				load->removeFromParent();
				function->replaceReferences(load->dest, newName);

				continue;
			}
		}
		else if (StoreInst* store = dynamic_cast<StoreInst*>(inst))
		{
			phiStack[store->dest].push(store->src);
			toPop.push_back(store->dest);

			inst = inst->next;
			store->removeFromParent();

			continue;
		}

		inst = inst->next;
	}

	// Fix-up phi nodes of successors
	for (BasicBlock* next : block->successors())
	{
		if (phis.find(next) != phis.end())
		{
			for (PhiDescription& phiDesc : phis.at(next))
			{
				if (!phiStack[phiDesc.original].empty())
				{
					phiDesc.sources.emplace_back(block, phiStack[phiDesc.original].top());
				}
				else
				{
					// It's possible that some predecessor doesn't define the variable at all.
					// This is probably an indication that the variable isn't live, and we
					// don't need the phi node, but let's postpone that analysis
					phiDesc.sources.emplace_back(block, nullptr);
				}
			}
		}
	}

	// Recurse
	for (BasicBlock* next : block->successors())
	{
		rename(function, next, phis);
	}

	// Remove names from the stack
	for (Value* value : toPop)
	{
		assert(!phiStack[value].empty());
		phiStack[value].pop();
	}
}

void insertPhis(Function* function, phis_t& phis)
{
	for (auto& item : phis)
	{
		BasicBlock* block = item.first;

		for (PhiDescription& phiDesc : item.second)
		{
			assert(phiDesc.dest);

			PhiInst* phi = new PhiInst(phiDesc.dest);
			for (auto& source : phiDesc.sources)
			{
				if (!source.second)
				{
					// For some path leading to this block, there is no definition
					// to merge in this phi node.

					// Case 1: Function argument -- in this case, there is an
					// implicit store at the beginning of the function. Add an
					// explicit load in the predecessor
					if (dynamic_cast<Argument*>(phiDesc.original))
					{
						Value* tmp = generateName(function, phiDesc.original);
						LoadInst* inst = new LoadInst(tmp, phiDesc.original);
						inst->insertBefore(source.first->last);
						source.second = tmp;
					}

					// Case 2: Local variable -- in this case, the value really is
					// undefined, so the result variable better be dead, or we'll
					// have undefined behavior
					else
					{
						assert(phiDesc.dest->uses.empty());
					}
				}

				if (phiDesc.dest->uses.empty())
				{
					Value* deadValue = phiDesc.dest;

					// If the result is dead, then we don't need the phi
					phi->dropReferences();
					delete phi;

					// And we don't need the value
					assert(!deadValue->definition);
					function->temps.erase(std::remove(function->temps.begin(), function->temps.end(), deadValue), function->temps.end());

					phi = nullptr;
					break;
				}

				phi->addSource(source.first, source.second);
			}

			if (phi)
				block->prepend(phi);
		}
	}
}

void eliminatePhis(Function* function)
{
	for (BasicBlock* block : function->blocks)
	{
		Instruction* inst = block->first;
		while (inst != nullptr)
		{
			if (PhiInst* phi = dynamic_cast<PhiInst*>(inst))
			{
				// Each predecessor will copy into this variable, and then the
				// phi node will be replaced with a copy from this variable to
				// the phi destination. This is usually overkill (compared to
				// just a single copy in each predecessor), but it helps avoid
				// problems in some corner cases, and the extra copies will
				// hopefully be coalesced during register allocation.
				Value* temp = function->makeTemp();

				for (auto& source : phi->sources())
				{
					BasicBlock* pred = source.first;
					Value* value = source.second;

					CopyInst* copy = new CopyInst(temp, value);
					copy->insertBefore(pred->last);
				}

				// And delete the phi node
				inst = inst->next;
				phi->replaceWith(new CopyInst(phi->dest, temp));
			}
			else
			{
				break;
			}
		}
	}
}

void analyzeFunction(Function* function)
{
	dom_t dom = findDominators(function);
	idom_t idom = getImmediateDominators(dom);
	df_t df = getDominanceFrontiers(idom);
	phis_t allPhis = calculatePhiNodes(function, df);
	rename(function, function->blocks[0], allPhis);
	insertPhis(function, allPhis);

	for (auto& local : function->locals)
	{
		assert(local->uses.empty());
	}
	function->locals.clear();

	eliminatePhis(function);
}

void generateDot(Function* function)
{
	std::string fname = std::string("dots/") + function->name + ".dot";
	fstream f(fname.c_str(), std::ios::out);

	f << "digraph {" << std::endl;
	f << "node[fontname=\"Inconsolata\"]" << std::endl;

	for (BasicBlock* block : function->blocks)
	{
		f << "L" << block->seqNumber << " [label=\"";
		f << block->str() << "\\l";

		Instruction* inst = block->first;
		while (inst != nullptr)
		{
			f << "    " << inst->str() << "\\l";
			inst = inst->next;
		}

		f << "\"]" << std::endl;

		auto successors = block->successors();

		if (!successors.empty())
		{
			f << "L" << block->seqNumber << " -> {";
			for (size_t i = 0; i < successors.size(); ++i)
			{
				if (i != 0)
					f << ", ";

				f << "L" << successors[i]->seqNumber;
			}
			f << "}" << std::endl;
		}
	}

	f << "}" << std::endl;
}

int main(int argc, char* argv[])
{
	if (argc < 1)
	{
		std::cerr << "Please specify a source file to compile." << std::endl;
		return 1;
	}

	if (strcmp(argv[1], "--noPrelude") == 0)
	{
		lastFile = true;

		if (argc < 2)
		{
			std::cerr << "Please specify a source file to compile." << std::endl;
			return 1;
		}

		yyin = fopen(argv[2], "r");
		if (yyin == nullptr)
		{
			std::cerr << "File " << argv[1] << " not found" << std::endl;
			return 1;
		}
	}
	else
	{
		mainFile = fopen(argv[1], "r");
		if (mainFile == nullptr)
		{
			std::cerr << "File " << argv[1] << " not found" << std::endl;
			return 1;
		}

		yyin = fopen("lib/prelude.spl", "r");
		if (yyin == nullptr)
		{
			std::cerr << "cannot find prelude.spl" << std::endl;
			return 1;
		}
	}

	int return_value = 1;

	AstContext context;
	Parser parser(context);
	try
	{
		parser.parse();
	}
	catch (LexerError& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		fclose(yyin);
		return 1;
	}

	ProgramNode* root = context.root();
	SemanticAnalyzer semant(root);
	bool semantic_success = semant.analyze();

	if (semantic_success)
	{
		TACContext context;
		TACCodeGen tacGen(&context);
		root->accept(&tacGen);

		return_value = 0;

		TACValidator validator(&context);
		if (validator.isValid())
		{
			MachineContext machineContext;

			for (Function* function : context.functions)
			{
				dumpFunction(function);
				analyzeFunction(function);
				dumpFunction(function);
				generateDot(function);

				MachineCodeGen codeGen(&machineContext, function);
				MachineFunction* mf = codeGen.getResult();

				std::cerr << "Machine code:" << std::endl;
				for (MachineBB* mbb : mf->blocks)
				{
					std::cerr << "label " << *mbb << ":" << std::endl;
					for (MachineInst* inst : mbb->instructions)
					{
						std::cerr << "\t" << *inst << std::endl;
					}
				}
				std::cerr << std::endl;

				RegAlloc regAlloc;
				regAlloc.run(mf);
				//regAlloc.dumpGraph();

				std::cerr << "After register allocation:" << std::endl;
				for (MachineBB* mbb : mf->blocks)
				{
					std::cerr << "label " << *mbb << ":" << std::endl;
					for (MachineInst* inst : mbb->instructions)
					{
						std::cerr << "\t" << *inst << std::endl;
					}
				}
				std::cerr << std::endl;

				std::cerr << "In nasm format:" << std::endl;
				AsmPrinter asmPrinter(std::cerr);
				asmPrinter.printFunction(mf);
			}
		}
	}

	fclose(yyin);
	return return_value;
}
