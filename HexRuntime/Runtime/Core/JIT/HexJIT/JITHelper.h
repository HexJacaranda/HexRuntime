#pragma once
#include "..\..\..\RuntimeAlias.h"
#include "..\..\Type\Type.h"
#include "..\..\Object\Object.h"
#include "..\..\Object\ArrayObject.h"
#include "..\..\InteriorPointer.h"
#include "..\..\Meta\MethodDescriptor.h"

namespace RTJ::Hex::JITCall
{
	//Use to declare JIT helper call, some may be linked to assembly

#define JIT_ASSEMBLY_CALL extern "C"

	RTO::Object* __stdcall NewObject(Type* type);

	RTO::ArrayObject* __stdcall NewSZArray(Type* elementType, Int32 count);

	RTO::ArrayObject* __stdcall NewArray(Type* elementType, Int32 dimensionCount, Int32* dimensions);

	void __stdcall ManagedCall(RTM::MethodDescriptor* methodDescriptor);

	void __stdcall WriteBarrierForRef(RTO::ObjectRef* field, RTO::Object* fieldValue);
	void __stdcall WriteBarrierForInteriorRef(InteriorPointer* source, InteriorPointer interiorPtr);

	RTO::ObjectRef __stdcall ReadBarrierForRef(RTO::ObjectRef* field);
	InteriorPointer __stdcall ReadBarrierForInteriorRef(InteriorPointer* source);
}