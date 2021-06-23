#pragma once
#include "..\..\..\RuntimeAlias.h"

namespace RTJ::Hex
{
	enum class SignatureArgumentKinds
	{
		Native,
		Managed
	};

	struct JITNativeSignatureArgument
	{
		RTString ArgumentName;
		SignatureArgumentKinds Kind;
	};

	struct JITNativeSignature
	{
		RTString MethodName;
		UInt8* Entry;
		Int32 ArgumentCount;
		JITNativeSignatureArgument* Arguments;
	};

#define JIT_NATIVE_SIGNATURE_DECLARE(NAME) \
	extern JITNativeSignatureArgument NAME##Args[]; \
	extern JITNativeSignature NAME##Signature
	

#define JIT_NATIVE_SIGNATURE_IMPL(NAME, ...) \
	JITNativeSignatureArgument NAME##Args[] = { ##__VA_ARGS__ }; \
	JITNativeSignature NAME##Signature = \
	{ \
		Text(#NAME), \
		(UInt8*)&NAME , \
		sizeof(NAME##Args) / sizeof(NAME##Args[0]), \
		(JITNativeSignatureArgument*)&NAME##Args \
	}

#define ARG_MANAGED(NAME) { Text(#NAME), SignatureArgumentKinds::Managed }

//Native fixed arguments always go first.
#define ARG_NATIVE(NAME) { Text(#NAME), SignatureArgumentKinds::Native }
}