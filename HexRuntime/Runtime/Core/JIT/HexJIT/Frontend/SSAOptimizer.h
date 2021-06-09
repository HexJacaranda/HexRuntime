#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "..\HexJITContext.h"
#include "..\JITMemory.h"

namespace RTJ::Hex
{
	class SSAOptimizer
	{
		static constexpr Int32 SpaceCount = 1024;
		JITMemory* mMemory;
		HexJITContext* mJITContext;	
		Int8* mTraversalSpace;
		//Fold binary
		TreeNode* FoldBinaryOpConstant(BinaryArithmeticNode* node);
		//Fold unary
		TreeNode* FoldUnaryOpConstant(UnaryArithmeticNode* node);
		//Fold comapre constant
		TreeNode* FoldCompareConstant(CompareNode* node);
		//Call for each root node of stmt
		void FoldConstant(TreeNode*& stmtRoot);
		//Prune the unnecessary BBIns to possibly eliminate dead code
		void PruneFlowGraph(BasicBlock* basicBlock);
	public:
		SSAOptimizer(HexJITContext* context);
		BasicBlock* Optimize();
	};
}