#pragma once
#include "..\..\..\RuntimeAlias.h"
#include "..\..\Object\Object.h"
#include "..\..\Object\ArrayObject.h"
#include "..\..\InteriorPointer.h"
#include "..\..\Meta\MethodDescriptor.h"
#include "..\..\Meta\TypeDescriptor.h"
#include "JITNativeSignature.h"

namespace RTJ::Hex::JITCall
{
	using namespace RTM;
	//Use to declare JIT helper call, some may be linked to assembly

#define JIT_ASSEMBLY_CALL extern "C"

	RTO::Object* __stdcall NewObject(Type* type);
	JIT_NATIVE_SIGNATURE_DECLARE(NewObject);
	
	RTO::ArrayObject* __stdcall NewSZArray(Type* elementType, Int32 count);
	JIT_NATIVE_SIGNATURE_DECLARE(NewSZArray);
	
	RTO::ArrayObject* __stdcall NewArray(Type* elementType, Int32 dimensionCount, RTO::ArrayObject* dimensions);
	JIT_NATIVE_SIGNATURE_DECLARE(NewArray);

	void __stdcall ManagedCall(RTM::MethodDescriptor* methodDescriptor);
	JIT_NATIVE_SIGNATURE_DECLARE(ManagedCall);

	void __stdcall WriteBarrierForRef(RTO::ObjectRef* field, RTO::Object* fieldValue);
	JIT_NATIVE_SIGNATURE_DECLARE(WriteBarrierForRef);

	void __stdcall WriteBarrierForInteriorRef(InteriorPointer* source, InteriorPointer interiorPtr);
	JIT_NATIVE_SIGNATURE_DECLARE(WriteBarrierForInteriorRef);

	RTO::ObjectRef __stdcall ReadBarrierForRef(RTO::ObjectRef* field);
	JIT_NATIVE_SIGNATURE_DECLARE(ReadBarrierForRef);

	InteriorPointer __stdcall ReadBarrierForInteriorRef(InteriorPointer* source);
	JIT_NATIVE_SIGNATURE_DECLARE(NewObject);
}