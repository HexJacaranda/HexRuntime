#include "LivenessAnalyzer.h"
#include "..\..\..\Exception\RuntimeException.h"

namespace RTJ::Hex
{
	void LivenessAnalyzer::BuildLivenessDuration()
	{
		BuildLivenessDurationPhaseOne();
		BuildLivenessDurationPhaseTwo();
	}

	void LivenessAnalyzer::BuildLivenessDurationPhaseOne()
	{
		for (auto&& basicBlock : mContext->BBs)
		{
			/* Here we can assume that all the nodes are flattened by Linearizer.
			 * All the depth should not be greater than 3 (maybe 4?)
			 */
			Statement* tail = basicBlock->Now;
			Int32 livenessMapIndex = 0;

			//Iterate over normal body
			while (tail != nullptr && tail->Next != nullptr)
			{
				tail = tail->Next;
				livenessMapIndex++;
			}
			//Condition statement
			if (basicBlock->BranchConditionValue != nullptr)
				livenessMapIndex++;

			auto&& liveness = basicBlock->Liveness = std::move(LivenessMapT(livenessMapIndex + 1));
			VariableSet liveSet{};

			//In reverse order
			if (basicBlock->BranchConditionValue != nullptr)
			{
				UpdateLiveSet(basicBlock->BranchConditionValue, basicBlock, liveSet);
				liveness[livenessMapIndex] = liveSet;
				livenessMapIndex--;
			}
			
			for (Statement* stmt = tail; stmt != nullptr; stmt = stmt->Prev)
			{
				if (stmt->Now != nullptr)
				{
					UpdateLiveSet(stmt->Now, basicBlock, liveSet);
					liveness[livenessMapIndex] = liveSet;
				}
				livenessMapIndex--;
			}
		}
	}

	void LivenessAnalyzer::BuildLivenessDurationPhaseTwo()
	{
		BitSet stableMap(mContext->BBs.size());

		while (!stableMap.IsOne())
		{
			for (Int32 i = 0; i < stableMap.Count(); i++)
			{
				auto&& basicBlock = mContext->BBs[i];
				//Reduce computation for stable BB
				if (!stableMap.Test(i) && !basicBlock->IsUnreachable())
				{
					auto&& oldIn = basicBlock->VariablesLiveIn;
					auto&& oldOut = basicBlock->VariablesLiveOut;

					auto newIn = basicBlock->VariablesUse | (oldOut - basicBlock->VariablesDef);
					auto newOut = VariableSet{};

					basicBlock->ForeachSuccessor(
						[&](auto successor)
						{
							newOut = std::move(newOut | successor->VariablesLiveIn);
						});

					/* We only compare newIn with oldIn because:
					* 1. Changes from LiveOut will be propagated to LiveIn
					* 2. LiveIn changes will be propagated to predecessors' LiveOut
					*/

					if (newIn != oldIn)
					{
						//Propagate to predecessors
						for (auto&& bbIn : basicBlock->BBIn)
							stableMap.SetZero(bbIn->Index);

						//Update to new set
						oldIn = std::move(newIn);
						oldOut = std::move(newOut);
					}
					else
						stableMap.SetOne(i);
				}
			}
		}
	}

	void LivenessAnalyzer::UpdateLiveSet(TreeNode* node, BasicBlock* currentBB, VariableSet& liveSet)
	{
		auto use = [&](UInt16 index)
		{
			currentBB->VariablesUse.Add(index);
			liveSet.Add(index);
		};

		auto kill = [&](UInt16 index)
		{
			currentBB->VariablesDef.Add(index);
			liveSet.Remove(index);
		};

		/* Need to go from top since Load is deeper than Store.
		* And for expression like a (kill) = a (use) + b. We need to maintain 
		* 'a' in liveSet
		*/
		TraverseTree(
			mContext->Traversal.Space,
			mContext->Traversal.Count,
			node, 
			[&](TreeNode* value) 
			{
				switch (value->Kind)
				{
				case NodeKinds::Store:
				{
					if (auto variable = GuardedDestinationExtract(value->As<StoreNode>()))
						kill(variable->LocalIndex);
					break;
				}
				case NodeKinds::Load:
				{
					if (auto variable = GuardedSourceExtract(value->As<LoadNode>()))
						use(variable->LocalIndex);
					break;
				}
				}
			});
	}

	LocalVariableNode* LivenessAnalyzer::GuardedDestinationExtract(StoreNode* store)
	{
		auto dest = store->Destination;

		if (dest->Is(NodeKinds::LocalVariable))
			return dest->As<LocalVariableNode>();
		return nullptr;
	}

	LocalVariableNode* LivenessAnalyzer::GuardedSourceExtract(LoadNode* store)
	{
		auto source = store->Source;

		if (source->Is(NodeKinds::LocalVariable))
			return source->As<LocalVariableNode>();
		return nullptr;
	}

	void LivenessAnalyzer::BuildTopologciallySortedBB(BasicBlock* bb, BitSet& visited)
	{
		visited.SetOne(bb->Index);

		bb->ForeachSuccessor([&](BasicBlock* successor) {
			if (!visited.Test(successor->Index))
				BuildTopologciallySortedBB(successor, visited);
			});

		//In reverse order
		mContext->TopologicallySortedBBs.push_back(bb);
	}

	void LivenessAnalyzer::BuildTopologciallySortedBB()
	{
		BitSet visit(mContext->BBs.size());
		while (!visit.IsOne())
		{
			Int32 index = visit.ScanBiggestUnsetIndex();
			BuildTopologciallySortedBB(mContext->BBs[index], visit);
		}
		std::reverse(mContext->TopologicallySortedBBs.begin(), mContext->TopologicallySortedBBs.end());
	}

	LivenessAnalyzer::LivenessAnalyzer(HexJITContext* context) :
		mContext(context)
	{
	}

	BasicBlock* LivenessAnalyzer::PassThrough()
	{
		BuildLivenessDuration();
		BuildTopologciallySortedBB();
		return mContext->BBs.front();
	}
}