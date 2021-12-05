#pragma once
#include "..\..\..\RuntimeAlias.h"
#include "..\..\Object\Object.h"
#include "..\..\InteriorPointer.h"
#include "..\..\Meta\MethodDescriptor.h"
#include "..\..\Meta\TypeDescriptor.h"
#include "..\..\Object\ArrayObject.h"
#include "..\..\Platform\PlatformSpecialization.h"

namespace RTJ::Hex::JITCall
{
	template<class FnT>
	struct GetArgumentCountOf {};
	template<class R, class...Args>
	struct GetArgumentCountOf<R(*)(Args...)>
	{
		static constexpr Int32 Value = sizeof...(Args);
	};

#ifndef X64
	template<class R, class...Args>
	struct GetArgumentCountOf<R(__stdcall*)(Args...)>
	{
		static constexpr Int32 Value = sizeof...(Args);
	};

	template<class R, class...Args>
	struct GetArgumentCountOf<R(__fastcall*)(Args...)>
	{
		static constexpr Int32 Value = sizeof...(Args);
	};
#endif


	using namespace RTM;
	//Use to declare JIT helper call, some may be linked to assembly

#define JIT_NATIVE __stdcall

#define JIT_CALLING_CONV(NAME) \
	extern Platform::PlatformCallingArgument NAME##CCArgs[GetArgumentCountOf<decltype(&NAME)>::Value + 1]; \

#define JIT_CALLING_CONV_IMPL(NAME) \
	Platform::PlatformCallingArgument NAME##CCArgs[]

#define JIT_ARG(SIZE, FLAG) Platform::PlatformCallingArgument { SIZE, FLAG }

#define JIT_ARG_I4 JIT_ARG(sizeof(Int32), Platform::CallingArgumentType::Integer)
#define JIT_ARG_P JIT_ARG(sizeof(void*), Platform::CallingArgumentType::Integer)
#define JIT_ARG_IP JIT_ARG(2 * sizeof(void*), Platform::CallingArgumentType::Integer)
#define JIT_NO_RET JIT_ARG(0, 0)

	RTO::Object* JIT_NATIVE NewObject(Type* type);
	JIT_CALLING_CONV(NewObject);

	RTO::ArrayObject* JIT_NATIVE NewSZArray(Type* elementType, Int32 count);
	JIT_CALLING_CONV(NewSZArray);

	RTO::ArrayObject* JIT_NATIVE NewArray(Type* elementType, RTO::ArrayObject* dimensions);
	JIT_CALLING_CONV(NewArray);

	RTO::ArrayObject* JIT_NATIVE NewArrayFast(Type* elementType, Int32 dimensionCount, Int32* dimensions);
	JIT_CALLING_CONV(NewArrayFast);

	void JIT_NATIVE ManagedDirectCall(RTM::MethodDescriptor* methodDescriptor);
	JIT_CALLING_CONV(ManagedDirectCall);

	void JIT_NATIVE ManagedVirtualCall(RTM::MethodDescriptor* methodDescriptor);
	JIT_CALLING_CONV(ManagedVirtualCall);

	void JIT_NATIVE ManagedInterfaceCall(RTM::MethodDescriptor* methodDescriptor);
	JIT_CALLING_CONV(ManagedInterfaceCall);

	void JIT_NATIVE WriteBarrierForRef(RTO::ObjectRef* field, RTO::Object* fieldValue);
	JIT_CALLING_CONV(WriteBarrierForRef);

	void JIT_NATIVE WriteBarrierForInteriorRef(InteriorPointer* source, InteriorPointer interiorPtr);
	JIT_CALLING_CONV(WriteBarrierForInteriorRef);

	RTO::ObjectRef JIT_NATIVE ReadBarrierForRef(RTO::ObjectRef* field);
	JIT_CALLING_CONV(ReadBarrierForRef);

	InteriorPointer JIT_NATIVE ReadBarrierForInteriorRef(InteriorPointer* source);
	JIT_CALLING_CONV(ReadBarrierForInteriorRef);

	template<auto function, Int32 length>
	static Platform::PlatformCallingConvention* GetCallingConventionOf(Platform::PlatformCallingArgument (&arguments)[length]) {
		static Platform::PlatformCallingConvention* convention =
			Platform::PlatformCallingConventionProvider<
			Platform::CallingConventions::JIT,
			Platform::CurrentPlatform>::GetConvention(arguments | std::views::all);

		return convention;
	}

#define CALLING_CONV_OF(NAME) JITCall::GetCallingConventionOf<(void*)&JITCall::##NAME>(JITCall::NAME##CCArgs)
}