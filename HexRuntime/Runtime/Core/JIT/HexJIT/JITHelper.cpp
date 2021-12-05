#include "JITHelper.h"

namespace RTJ::Hex::JITCall
{
	JIT_CALLING_CONV_IMPL(NewObject) = { JIT_ARG_P, JIT_ARG_P };

	JIT_CALLING_CONV_IMPL(NewSZArray) = { JIT_ARG_P, JIT_ARG_P, JIT_ARG_I4 };

	JIT_CALLING_CONV_IMPL(NewArray) = { JIT_ARG_P, JIT_ARG_P, JIT_ARG_P };

	JIT_CALLING_CONV_IMPL(NewArrayFast) = { JIT_ARG_P, JIT_ARG_P, JIT_ARG_I4, JIT_ARG_P };

	JIT_CALLING_CONV_IMPL(ManagedDirectCall) = { JIT_NO_RET, JIT_ARG_P };

	JIT_CALLING_CONV_IMPL(ManagedVirtualCall) = { JIT_NO_RET, JIT_ARG_P };

	JIT_CALLING_CONV_IMPL(ManagedInterfaceCall) = { JIT_NO_RET, JIT_ARG_P };

	JIT_CALLING_CONV_IMPL(WriteBarrierForRef) = { JIT_NO_RET, JIT_ARG_P, JIT_ARG_P };

	JIT_CALLING_CONV_IMPL(WriteBarrierForInteriorRef) = { JIT_NO_RET, JIT_ARG_P, JIT_ARG_IP };

	JIT_CALLING_CONV_IMPL(ReadBarrierForRef) = { JIT_NO_RET, JIT_ARG_P };

	JIT_CALLING_CONV_IMPL(ReadBarrierForInteriorRef) = { JIT_NO_RET, JIT_ARG_P };
}

RTO::Object* JIT_NATIVE RTJ::Hex::JITCall::NewObject(Type* type)
{
	return nullptr;
}

RTO::ArrayObject* JIT_NATIVE RTJ::Hex::JITCall::NewSZArray(Type* elementType, Int32 count)
{
	return nullptr;
}

RTO::ArrayObject* JIT_NATIVE RTJ::Hex::JITCall::NewArray(Type* elementType, RTO::ArrayObject* dimensions)
{
	if (!dimensions->IsSZArray())
	{

	}
	return nullptr;
}

void JIT_NATIVE RTJ::Hex::JITCall::ManagedDirectCall(RTM::MethodDescriptor* methodDescriptor)
{
}

void JIT_NATIVE RTJ::Hex::JITCall::ManagedVirtualCall(RTM::MethodDescriptor* methodDescriptor)
{
}

void JIT_NATIVE RTJ::Hex::JITCall::ManagedInterfaceCall(RTM::MethodDescriptor* methodDescriptor)
{
}

void JIT_NATIVE RTJ::Hex::JITCall::WriteBarrierForRef(RTO::ObjectRef* field, RTO::Object* fieldValue)
{
}

void JIT_NATIVE RTJ::Hex::JITCall::WriteBarrierForInteriorRef(InteriorPointer* source, InteriorPointer interiorPtr)
{
}

RTO::ObjectRef JIT_NATIVE RTJ::Hex::JITCall::ReadBarrierForRef(RTO::ObjectRef* field)
{
	return RTO::ObjectRef();
}

RTC::InteriorPointer JIT_NATIVE RTJ::Hex::JITCall::ReadBarrierForInteriorRef(InteriorPointer* source)
{
	return InteriorPointer();
}

RTO::ArrayObject* JIT_NATIVE RTJ::Hex::JITCall::NewArrayFast(Type* elementType, Int32 dimensionCount, Int32* dimensions)
{
	return nullptr;
}