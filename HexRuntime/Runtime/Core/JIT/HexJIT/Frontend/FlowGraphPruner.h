#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "..\..\..\Memory\SegmentHeap.h"
#include "..\HexJITContext.h"
#include "..\JITFlow.h"
#include "..\IR.h"

namespace RTJ::Hex
{
	class FlowGraphPruner : public IHexJITFlow 
	{
		HexJITContext* mContext;
		BasicBlock* mHead;
		//Prune the unnecessary BBIns to possibly eliminate dead code
		void PruneFlowGraph(BasicBlock* basicBlock);
		void MarkBB();
		void MarkBB(SmallSet<Int32>& visited, BasicBlock* bb);
		void FixBBRelation();
	public:
		FlowGraphPruner(HexJITContext* context);
		virtual BasicBlock* PassThrough() final;
	};
}