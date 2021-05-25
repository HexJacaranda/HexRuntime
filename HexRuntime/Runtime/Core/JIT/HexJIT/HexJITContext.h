#pragma once
#include "..\..\..\RuntimeAlias.h"
#include "Frontend/IR.h"
#include <vector>

namespace RTJ::Hex
{
	struct RWRecord
	{
		UInt8 Flag;
		TreeNode** UpdatePoint;
	};

	struct HexJITContext
	{
		/// <summary>
		/// Indicate the trackability of local variable
		/// </summary>
		bool* LocalVariableSSATrackable;
		/// <summary>
		/// Indicate the trackability of arugments
		/// </summary>
		bool* ArgumentsVariableSSATrackable;
		/// <summary>
		/// Record the writes/reads of local/argument variables
		/// </summary>
		std::vector<RWRecord> SSARWRecords;
	};
}