#pragma once
#include "..\..\..\RuntimeAlias.h"
#include "..\..\..\Utility.h"
#include "Frontend\IR.h"
#include "..\JITContext.h"
#include <vector>

namespace RTJ::Hex
{
	enum class SSATrackability
	{
		Pending,
		Ok,
		Forbidden,
	};

	struct HexJITContext
	{
		JITContext* Context;
		/// <summary>
		/// Indicate the trackability of local variable
		/// </summary>
		std::vector<SSATrackability> LocalSSATrackability;
		/// <summary>
		/// Indicate the trackability of arugments
		/// </summary>
		std::vector<SSATrackability> ArgumentSSATrackability;
		/// <summary>
		/// Index to basic block
		/// </summary>
		std::vector<BasicBlock*> BBs;
	};
}