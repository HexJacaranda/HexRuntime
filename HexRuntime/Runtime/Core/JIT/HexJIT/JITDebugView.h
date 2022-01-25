#pragma once
#include "..\..\..\RuntimeAlias.h"
#include "IR.h"
#include "Backend\RegisterAllocationContext.h"
#include <string>

namespace RTJ::Hex
{
	class JITDebugView
	{
		static void ViewIRNodeContent(std::wstringstream& output, TreeNode* node);
		static void ViewIRNode(std::wstringstream& output, std::wstring const& prefix, TreeNode* node, bool isLast);
	public:
		static std::wstring ViewIR(BasicBlock* bb);
		static std::wstring ViewIR(TreeNode* node);
		static std::wstring ViewRegisterContext(RegisterAllocationContext* context);
	};
}