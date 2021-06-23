#pragma once
#include "..\..\..\RuntimeAlias.h"
#include "..\..\Type\Type.h"
#include "..\..\Object\Object.h"
#include "..\..\Object\ArrayObject.h"
#include "..\..\InteriorPointer.h"
#include "..\..\Meta\MethodDescriptor.h"
#include "JITNativeSignature.h"

namespace RTJ::Hex::JITCall
{
	//Use to declare JIT helper call, some may be linked to assembly

#define JIT_ASSEMBLY_CALL extern "C"

	RTO::Object* __stdcall NewObject(Type* type);
	JIT_NATIVE_SIGNATURE(NewObject, ARG_NATIVE(type));

	RTO::ArrayObject* __stdcall NewSZArray(Type* elementType, Int32 count);
	JIT_NATIVE_SIGNATURE(NewSZArray, ARG_NATIVE(elementType), ARG_MANAGED(count));

	RTO::ArrayObject* __stdcall NewArray(Type* elementType, Int32 dimensionCount, RTO::ArrayObject* dimensions);
	JIT_NATIVE_SIGNATURE(NewArray, ARG_NATIVE(elementType), ARG_MANAGED(dimensionCount), ARG_MANAGED(dimensions));

	void __stdcall ManagedCall(RTM::MethodDescriptor* methodDescriptor);
	JIT_NATIVE_SIGNATURE(ManagedCall, ARG_NATIVE(methodDescriptor));

	void __stdcall WriteBarrierForRef(RTO::ObjectRef* field, RTO::Object* fieldValue);
	JIT_NATIVE_SIGNATURE(WriteBarrierForRef, ARG_MANAGED(field), ARG_MANAGED(fieldValue));

	void __stdcall WriteBarrierForInteriorRef(InteriorPointer* source, InteriorPointer interiorPtr);
	JIT_NATIVE_SIGNATURE(WriteBarrierForInteriorRef, ARG_MANAGED(source), ARG_MANAGED(interiorPtr));

	RTO::ObjectRef __stdcall ReadBarrierForRef(RTO::ObjectRef* field);
	JIT_NATIVE_SIGNATURE(ReadBarrierForRef, ARG_MANAGED(field));
	InteriorPointer __stdcall ReadBarrierForInteriorRef(InteriorPointer* source);
	JIT_NATIVE_SIGNATURE(ReadBarrierForInteriorRef, ARG_MANAGED(source));
}