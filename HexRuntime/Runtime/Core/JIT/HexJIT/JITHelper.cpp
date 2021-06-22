#include "JITHelper.h"

RTO::Object* __stdcall RTJ::Hex::JITCall::NewObject(Type* type)
{
	return nullptr;
}

RTO::ArrayObject* __stdcall RTJ::Hex::JITCall::NewSZArray(Type* elementType, Int32 count)
{
	return nullptr;
}

RTO::ArrayObject* __stdcall RTJ::Hex::JITCall::NewArray(Type* elementType, Int32 dimensionCount, Int32* dimensions)
{
	return nullptr;
}

void __stdcall RTJ::Hex::JITCall::ManagedCall(RTM::MethodDescriptor* methodDescriptor)
{
}

void __stdcall RTJ::Hex::JITCall::WriteBarrierForRef(RTO::ObjectRef* field, RTO::Object* fieldValue)
{
}

void __stdcall RTJ::Hex::JITCall::WriteBarrierForInteriorRef(InteriorPointer* source, InteriorPointer interiorPtr)
{
}

RTO::ObjectRef __stdcall RTJ::Hex::JITCall::ReadBarrierForRef(RTO::ObjectRef* field)
{
	return RTO::ObjectRef();
}

RTC::InteriorPointer __stdcall RTJ::Hex::JITCall::ReadBarrierForInteriorRef(InteriorPointer* source)
{
	return InteriorPointer();
}
