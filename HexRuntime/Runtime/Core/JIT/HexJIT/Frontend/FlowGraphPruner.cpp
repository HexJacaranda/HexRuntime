#include "FlowGraphPruner.h"
#include "..\..\..\Exception\RuntimeException.h"
#include "..\..\..\..\Bit.h"

namespace RTJ::Hex {

	void RemoveBBFrom(std::vector<BasicBlock*>& bbIn, BasicBlock* target) {
		auto me = std::find(bbIn.begin(), bbIn.end(), target);
		if (me != bbIn.end()) bbIn.erase(me);
	}

	void FlowGraphPruner::PruneFlowGraph(BasicBlock* basicBlock)
	{
		//Only for conditional now
		if (basicBlock->BranchKind == PPKind::Conditional)
		{
			//Ref
			auto&& conditionValue = basicBlock->BranchConditionValue;
			if (conditionValue->Is(NodeKinds::Constant))
			{
				//If this is true, then it becomes an unconditional jump
				auto constant = conditionValue->As<ConstantNode>();
				BasicBlock* abandonedTarget = nullptr;
				//If this is true, then we will jump to the target BB unconditionally
				if (constant->Bool)
				{
					abandonedTarget = basicBlock->Next;
					//Set to unconditional
					basicBlock->BranchKind = PPKind::Unconditional;
				}
				else
				{
					abandonedTarget = basicBlock->BranchedBB;
					basicBlock->BranchKind = PPKind::Sequential;
				}
				//Remove BBIn from abandoned target BB
				RemoveBBFrom(abandonedTarget->BBIn, basicBlock);
			}
		}
	}

	void FlowGraphPruner::MarkBB()
	{
		SmallSet<Int32> visitedSet{};
		MarkBB(visitedSet, mHead);
	}

	void FlowGraphPruner::MarkBB(SmallSet<Int32>& visited, BasicBlock* bb)
	{
		if (visited.Contains(bb->Index))
			return;

		visited.Add(bb->Index);

		//Unreachable, dead here
		if (bb != mHead && bb->BBIn.size() == 0)
			return;

		bb->Flags |= BBFlags::Reachable;
		bb->ForeachSuccessor(
			[&](BasicBlock* successor)
			{
				MarkBB(visited, successor);
			});
	}

	void FlowGraphPruner::FixBBRelation()
	{
		std::vector<BasicBlock*> aliveBBs{};

		for (auto&& bb : mContext->BBs | std::views::filter(BasicBlock::ReachableFn))
		{
			aliveBBs.push_back(bb);
			//Remove unreachable BB from BBIn
			for (auto it = bb->BBIn.begin(); it != bb->BBIn.end(); )
			{
				if ((*it)->IsUnreachable())
					it = bb->BBIn.erase(it);
				else
					++it;
			}
		}

		//There may be holes between two alive BB, which may still requires unconditional jump
		for (Int32 i = 1; i < aliveBBs.size(); ++i)
		{
			auto&& first = aliveBBs[i - 1];
			auto&& second = aliveBBs[i];

			if (first->BranchKind == PPKind::Unconditional && first->BranchedBB == second)
			{
				first->BranchKind = PPKind::Sequential;
				first->BranchedBB = nullptr;
				first->Next = second;
			}
		}

		mContext->AliveBBs = std::move(aliveBBs);
	}

	FlowGraphPruner::FlowGraphPruner(HexJITContext* context)
		:mContext(context), mHead(context->BBs.front())
	{
	}

	BasicBlock* FlowGraphPruner::PassThrough()
	{
		for (auto&& bb : mContext->BBs)
			PruneFlowGraph(bb);
	}
}
