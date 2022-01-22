#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "..\..\..\..\Bit.h"
#include "..\JITFlow.h"

namespace RTJ::Hex
{
	class LivenessAnalyzer : public IHexJITFlow
	{
		HexJITContext* mContext = nullptr;
	private:
		void BuildLivenessDuration();
		/// <summary>
		/// Build the main part of liveness
		/// </summary>
		void BuildLivenessDurationPhaseOne();
		/// <summary>
		/// Build the rest
		/// </summary>
		void BuildLivenessDurationPhaseTwo();
		/// <summary>
		/// Update live set
		/// </summary>
		/// <param name="node"></param>
		/// <param name="currentBB"></param>
		/// <param name="liveSet"></param>
		void UpdateLiveSet(TreeNode* node, BasicBlock* currentBB, VariableSet& liveSet);
		static LocalVariableNode* GuardedDestinationExtract(StoreNode* store);
		static LocalVariableNode* GuardedSourceExtract(LoadNode* store);

		void BuildTopologciallySortedBB(BasicBlock* bb, BitSet& visited);
		void BuildTopologciallySortedBB(void);
	public:
		LivenessAnalyzer(HexJITContext* context);
		virtual BasicBlock* PassThrough() final;
	};
}