#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "..\JITFlow.h"

namespace RTJ::Hex {
	class StoreElimination : public IHexJITFlow {
		HexJITContext* mJITContext;
		bool HasSideEffect(TreeNode* root);
	public:
		StoreElimination(HexJITContext* context);
		virtual BasicBlock* PassThrough();
	};
}
