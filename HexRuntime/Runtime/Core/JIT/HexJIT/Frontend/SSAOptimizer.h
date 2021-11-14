#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "..\..\..\Memory\SegmentMemory.h"
#include "..\HexJITContext.h"
#include "..\JITFlow.h"
#include "..\IR.h"


namespace RTJ::Hex
{
	class SSAOptimizer : public IHexJITFlow
	{
		RTMM::SegmentMemory* mMemory;
		HexJITContext* mJITContext;	
		//Fold binary
		TreeNode* FoldBinaryOpConstant(BinaryArithmeticNode* node);
		//Fold unary
		TreeNode* FoldUnaryOpConstant(UnaryArithmeticNode* node);
		//Fold comapre constant
		TreeNode* FoldCompareConstant(CompareNode* node);
		//Fold value use
		TreeNode* FoldValueUse(SSA::ValueUse* use);
		//Fold value def
		TreeNode* FoldValueDef(SSA::ValueDef* def);
		//Generic fold
		TreeNode* Fold(TreeNode* node);
		//Call for each root node of stmt
		void FoldConstant(TreeNode*& stmtRoot);
		//Prune the unnecessary BBIns to possibly eliminate dead code
		void PruneFlowGraph(BasicBlock* basicBlock);
	public:
		SSAOptimizer(HexJITContext* context);
		virtual BasicBlock* PassThrough() final;
	};
}