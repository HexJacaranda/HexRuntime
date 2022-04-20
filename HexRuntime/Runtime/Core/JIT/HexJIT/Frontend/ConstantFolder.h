#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "..\..\..\Memory\SegmentHeap.h"
#include "..\HexJITContext.h"
#include "..\JITFlow.h"
#include "..\IR.h"


namespace RTJ::Hex
{
	class ConstantFolder : public IHexJITFlow
	{
		RTMM::SegmentHeap* mMemory;
		HexJITContext* mJITContext;	
		//Fold binary
		TreeNode* Fold(BinaryArithmeticNode* node);
		//Fold unary
		TreeNode* Fold(UnaryArithmeticNode* node);
		//Fold comapre constant
		TreeNode* Fold(CompareNode* node);
		//Fold value use
		TreeNode* Fold(SSA::ValueUse* use);
		//Fold value def
		TreeNode* Fold(SSA::ValueDef* def);
		//Generic fold
		TreeNode* Fold(TreeNode* node);
		//Fold convert
		TreeNode* Fold(ConvertNode* node);
		//Call for each root node of stmt
		void FoldConstant(TreeNode*& stmtRoot);
	public:
		ConstantFolder(HexJITContext* context);
		virtual BasicBlock* PassThrough() final;
	};
}