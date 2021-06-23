#include "JITHelper.h"

namespace RTJ::Hex::JITCall
{
	JIT_NATIVE_SIGNATURE_IMPL(NewObject, ARG_NATIVE(type));
	JIT_NATIVE_SIGNATURE_IMPL(NewSZArray, ARG_NATIVE(elementType), ARG_MANAGED(count));
	JIT_NATIVE_SIGNATURE_IMPL(NewArray, ARG_NATIVE(elementType), ARG_MANAGED(dimensionCount), ARG_MANAGED(dimensions));
	JIT_NATIVE_SIGNATURE_IMPL(ManagedCall, ARG_NATIVE(methodDescriptor));
	JIT_NATIVE_SIGNATURE_IMPL(WriteBarrierForRef, ARG_MANAGED(field), ARG_MANAGED(fieldValue));
	JIT_NATIVE_SIGNATURE_IMPL(WriteBarrierForInteriorRef, ARG_MANAGED(source), ARG_MANAGED(interiorPtr));
	JIT_NATIVE_SIGNATURE_IMPL(ReadBarrierForRef, ARG_MANAGED(field));
	JIT_NATIVE_SIGNATURE_IMPL(ReadBarrierForInteriorRef, ARG_MANAGED(source));
}


RTO::Object* __stdcall RTJ::Hex::JITCall::NewObject(Type* type)
{
	return nullptr;
}

RTO::ArrayObject* __stdcall RTJ::Hex::JITCall::NewSZArray(Type* elementType, Int32 count)
{
	return nullptr;
}

RTO::ArrayObject* __stdcall RTJ::Hex::JITCall::NewArray(Type* elementType, Int32 dimensionCount, RTO::ArrayObject* dimensions)
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
