#include "SSABuilder.h"
#include "..\..\..\Meta\CoreTypes.h"
#include "..\..\..\Meta\MethodDescriptor.h"

#define NEW(TYPE) mJITContext->Memory->New<TYPE>
#define POOL mJITContext->Memory

void RTJ::Hex::SSABuilder::DecideSSATrackability()
{
	/*
	 * Usually we won't track any type modified with pointer or
	 * marked not primitive. But the nullability of reference
	 * is involved in tracking for eliminating redundant checks
	 */

	auto trackableSign = [](UInt8 type) {
		if (CoreTypes::IsPrimitive(type) || type == CoreTypes::Ref)
			return LocalAttachedFlags::Trackable;
		return 0u;
	};

	auto rawContext = mJITContext->Context;

	auto localVariables = rawContext->MethDescriptor->GetLocalVariables();
	auto arguments = rawContext->MethDescriptor->GetSignature()->GetArguments();

	for (Int32 i = 0; i < localVariables.Count; ++i)
		mJITContext->LocalAttaches[i].Flags |= trackableSign(localVariables[i].GetType()->GetCoreType());

	for (Int32 i = 0; i < arguments.Count; ++i)
		mJITContext->ArgumentAttaches[i].Flags |= trackableSign(arguments[i].GetType()->GetCoreType());
}

void RTJ::Hex::SSABuilder::InitializeLocalsAndArguments()
{
	/* Currently we don't support skipping initialization of locals (maybe later)
	* So UndefinedValue won't occur in local loading
	*/
	constexpr Int32 basicBlockIndex = 0;
	for (Int32 i = 0; i < mJITContext->ArgumentAttaches.size(); ++i)
		mVariableDefinition[(UInt16)i | LocalVariableNode::ArgumentFlag][basicBlockIndex] =
		new (POOL) SSA::ValueDef(&SSA::UndefinedValueNode::Instance(), new (POOL) ArgumentNode(i));

	//Method to get default value node
	auto getDefaultValue = [&](Int32 index) -> TreeNode* {
		auto type = mJITContext->LocalAttaches[index].GetType();
		auto coreType = type->GetCoreType();
		if (CoreTypes::IsPrimitive(coreType))
		{
			auto node = new (POOL) ConstantNode(coreType);
			node->I8 = 0ll;
			return node;
		}

		if (CoreTypes::IsRef(coreType))
			return &NullNode::Instance();

		//TODO: How about struct?
		return &NullNode::Instance();
	};

	for (Int32 i = 0; i < mJITContext->LocalAttaches.size(); ++i)
		mVariableDefinition[(UInt16)i][basicBlockIndex] =
		new (POOL) SSA::ValueDef(getDefaultValue(i), new (POOL) LocalVariableNode(i));
}

bool RTJ::Hex::SSABuilder::IsVariableTrackable(LocalVariableNode* local)
{
	auto index = local->GetIndex();

	if (local->IsArgument())
		return mJITContext->ArgumentAttaches[index].IsTrackable();
	else
		return mJITContext->LocalAttaches[index].IsTrackable();
}

void RTJ::Hex::SSABuilder::WriteVariable(LocalVariableNode* local, Int32 blockIndex, TreeNode* value)
{
	auto use = new (POOL) SSA::ValueDef(value, local);
	mVariableDefinition[local->LocalIndex][blockIndex] = use;
}

bool RTJ::Hex::SSABuilder::IsUseUndefined(TreeNode* node)
{
	if (node->Is(NodeKinds::ValueUse))
	{
		auto use = node->As<SSA::ValueUse>();
		if (use->Def->Value->Is(NodeKinds::UndefinedValue))
			return true;
	}
	return false;
}

RTJ::Hex::TreeNode* RTJ::Hex::SSABuilder::TrackPhiUsage(TreeNode* value)
{
	if (!value->Is(NodeKinds::Phi))
		return value;

	auto phiNode = value->As<SSA::PhiNode>();
	auto position = mPhiUsage.find(phiNode);
	auto newNode = new (POOL) SSA::Use(value);
	if (position == mPhiUsage.end())
		mPhiUsage[phiNode] = newNode;
	else
	{
		newNode->Prev = position->second;
		position->second = newNode;
	}

	return newNode;
}

void RTJ::Hex::SSABuilder::UpdatePhiUsage(SSA::PhiNode* origin, TreeNode* target)
{
	auto iterator = mPhiUsage.find(origin);
	if (iterator == mPhiUsage.end())
		return;

	auto usage = iterator->second;
	while (usage != nullptr)
	{
		usage->Value = target;
		usage = usage->Prev;
	}

	//Erase
	iterator->second = nullptr;
}

RTJ::Hex::SSA::ValueUse* RTJ::Hex::SSABuilder::UseDef(SSA::ValueDef* def)
{
	auto use = new (POOL) SSA::ValueUse(def);
	//Increment
	def->Count++;
	
	//Hang it to use chain
	use->Prev = def->UseChain;
	def->UseChain = use;

	return use;
}

RTJ::Hex::TreeNode* RTJ::Hex::SSABuilder::ReadVariable(LocalVariableNode* local, Int32 blockIndex)
{
	TreeNode* ret = nullptr;
	SSA::ValueDef* def = nullptr;

	def = mVariableDefinition[local->LocalIndex][blockIndex];
	if (def != nullptr)
		ret = UseDef(def);
	else
		ret = ReadVariableLookUp(local, blockIndex);

	return TrackPhiUsage(ret);
}

RTJ::Hex::TreeNode* RTJ::Hex::SSABuilder::ReadVariableLookUp(LocalVariableNode* local, Int32 blockIndex)
{
	BasicBlock* block = mJITContext->BBs[blockIndex];
	TreeNode* value = nullptr;
	if (block->BBIn.size() == 1 && block->BBIn[0] != nullptr)
		value = ReadVariable(local, block->BBIn[0]->Index);
	else
	{
		auto phi = mJITContext->Memory->New<SSA::PhiNode>(block, local);
		WriteVariable(local, blockIndex, phi);
		value = AddPhiOperands(local, blockIndex, phi);
	}
	WriteVariable(local, blockIndex, value);
	return value;
}

RTJ::Hex::TreeNode* RTJ::Hex::SSABuilder::TrySimplifyChoice(SSA::PhiNode* origin, TreeNode* node)
{
	if (origin == node)
		return nullptr;
	if (node->Is(NodeKinds::Phi))
	{
		auto phiChoice = node->As<SSA::PhiNode>();
		if (phiChoice->IsEmpty() || phiChoice->IsRemoved())
			return nullptr;
	}
	//Non-trival case
	return node;
}

RTJ::Hex::TreeNode* RTJ::Hex::SSABuilder::AddPhiOperands(LocalVariableNode* local, Int32 blockIndex, SSA::PhiNode* phiNode)
{
	BasicBlock* block = mJITContext->BBs[blockIndex];
	for (auto&& predecessor : block->BBIn)
	{
		auto choice = TrySimplifyChoice(phiNode, ReadVariable(local, predecessor->Index));
		if (choice != nullptr)
		{
			phiNode->Choices.push_back(choice);
			TrackPhiUsage(choice);
		}
	}
		
	return TryRemoveRedundantPhiNode(phiNode);
}

RTJ::Hex::TreeNode* RTJ::Hex::SSABuilder::TryRemoveRedundantPhiNode(SSA::PhiNode* phiNode)
{
	//Deal with duplicated case
	TreeNode* sameNode = nullptr;
	for (auto&& choice : phiNode->Choices)
	{
		if (choice == sameNode || choice == phiNode)
			continue;
		if (sameNode != nullptr)
		{
			//Non-trivial case, merged two or more values that may not collapse
			return phiNode;
		}
		sameNode = choice;
	}
	
	//Update
	UpdatePhiUsage(phiNode, sameNode);
	return sameNode;
}

RTJ::Hex::SSABuilder::SSABuilder(HexJITContext* jitContext) :
	mJITContext(jitContext),
	mTarget(jitContext->BBs.front()) 
{

}

RTJ::Hex::BasicBlock* RTJ::Hex::SSABuilder::PassThrough()
{
	//Initialize
	DecideSSATrackability();
	InitializeLocalsAndArguments();

	auto readWrite = [&](TreeNode*& node, Int32 bbIndex) {
		//The store node is always a stmt
		if (node->Kind == NodeKinds::Store)
		{
			auto store = node->As<StoreNode>();
			auto kind = store->Destination->Kind;
			auto index = -1;

			LocalVariableNode* local = nullptr;
			if (kind == NodeKinds::LocalVariable)
			{
				local = store->Destination->As<LocalVariableNode>();
				index = local->LocalIndex;
			}
				
			if (index != -1 && IsVariableTrackable(local))
				WriteVariable(local, bbIndex, store->Source);
		}

		TraverseTreeBottomUp(
			mJITContext->Traversal.Space,
			mJITContext->Traversal.Count,
			node, 
			[&](TreeNode*& value) {
				if (value->Is(NodeKinds::Load))
				{
					auto load = value->As<LoadNode>();
					if (load->Source->Is(NodeKinds::LocalVariable) &&
						load->Mode == AccessMode::Value)
					{

					}
				}
				if (ValueIs(value, NodeKinds::LocalVariable))
				{
					auto local = ValueAs<LocalVariableNode>(value);
					if (IsVariableTrackable(local)) {
						auto readValue = ReadVariable(local, bbIndex);
						if (IsUseUndefined(readValue))
							//Use origin node
							value = local;
						else
							value = readValue;
					}
				}
			});
	};

	//Traverse to find every write or read for local variables marked trackable.
	for (BasicBlock* bbIterator = mTarget; 
		bbIterator != nullptr; 
		bbIterator = bbIterator->Next)
	{
		for (Statement* stmtIterator = bbIterator->Now;
			stmtIterator != nullptr && stmtIterator->Now != nullptr;
			stmtIterator = stmtIterator->Next)
		{
			readWrite(stmtIterator->Now, bbIterator->Index);
		}
		//Flow control value may also involve r/w
		if (bbIterator->BranchConditionValue != nullptr)
			readWrite(bbIterator->BranchConditionValue, bbIterator->Index);
	}
	return mTarget;
}
