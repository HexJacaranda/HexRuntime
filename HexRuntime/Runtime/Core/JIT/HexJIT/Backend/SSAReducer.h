#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "..\HexJITContext.h"
#include "..\JITFlow.h"
#include "..\IR.h"

namespace RTJ::Hex
{
	/// <summary>
	/// Reducing IR from SSA to normal form
	/// </summary>
	class SSAReducer : public IHexJITFlow
	{
		HexJITContext* mContext;
	private:
		TreeNode* Reduce(SSA::ValueUse* use);
		TreeNode* Reduce(SSA::ValueDef* def);
		TreeNode* Reduce(SSA::PhiNode* phi);
		TreeNode* Reduce(SSA::Use* use);
		TreeNode* Reduce(TreeNode* node);
	public:
		SSAReducer(HexJITContext* context);
		virtual BasicBlock* PassThrough() final;
	};
}