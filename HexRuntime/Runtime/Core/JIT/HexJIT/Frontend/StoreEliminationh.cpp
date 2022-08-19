#include "StoreEliminationh.h"

RTJ::Hex::StoreElimination::StoreElimination(HexJITContext* context)
    :mJITContext(context)
{
}

bool RTJ::Hex::StoreElimination::HasSideEffect(TreeNode* root)
{
    bool sideEffect = false;
	TraverseTreeWithInterruptableFn(
		mJITContext->Traversal.Space,
		mJITContext->Traversal.Count,
		root,
		[&](TreeNode*& value) {
            if (value->Is(NodeKinds::MorphedCall)) {
                sideEffect = true;
                return false;
            }
            return true;
		});
    return sideEffect;
}

RTJ::Hex::BasicBlock* RTJ::Hex::StoreElimination::PassThrough()
{
    for (auto&& [varaible, defMap] : mJITContext->SSAVariableDefinition)
    {
        for (auto&& [bbIndex, def] : defMap) 
        {
            if (def->Count == 0 && !HasSideEffect(def->StoreStmt->Now)) {
                //Can be removed
                auto bb = mJITContext->BBs[bbIndex];
                LinkedList::RemoveTwoWay(bb->Now, def->StoreStmt);
            }
        }
    }
    return mJITContext->BBs.front();
}
