#include "IR.h"
#include "..\..\Meta\TypeDescriptor.h"

bool RTJ::Hex::TreeNode::CheckEquivalentWith(TreeNode* target) const
{
	return TypeInfo == target->TypeInfo;
}

bool RTJ::Hex::TreeNode::CheckEquivalentWith(RTM::TypeDescriptor* target) const
{
	return TypeInfo == target;
}

bool RTJ::Hex::TreeNode::CheckUpCastFrom(TreeNode* target) const
{
	return CheckUpCastFrom(target->TypeInfo);
}

bool RTJ::Hex::TreeNode::CheckUpCastFrom(RTM::TypeDescriptor* source) const
{
	if (TypeInfo == nullptr && source == nullptr)
		return true;
	if (TypeInfo == nullptr)
		return false;
	return TypeInfo->IsAssignableFrom(source);
}

void RTJ::Hex::TreeNode::TypeFrom(TreeNode* target)
{
	TypeInfo = target->TypeInfo;
}
