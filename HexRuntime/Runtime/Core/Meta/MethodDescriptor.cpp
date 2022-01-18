#include "MethodDescriptor.h"

RT::UInt8 RTM::MethodDescriptor::GetAccessbility() const
{
	return mColdMD->Accessibility;
}

RTM::MethodSignatureDescriptor * RTM::MethodDescriptor::GetSignature() const
{
	return mSignature;
}

RT::ObservableArray<RTM::MethodLocalVariableDescriptor> RTM::MethodDescriptor::GetLocalVariables() const
{
	return { mLocals,  mColdMD->ILCodeMD.LocalVariableCount };
}

RTME::ILMD* RTM::MethodDescriptor::GetIL() const
{
	return &mColdMD->ILCodeMD;
}

RTM::TypeDescriptor* RTM::MethodDescriptor::GetReturnType() const
{
	return GetSignature()->GetReturnType();
}

RT::ObservableArray<RTM::MethodArgumentDescriptor> RTM::MethodDescriptor::GetArguments() const
{
	return GetSignature()->GetArguments();
}

RT::MDToken RTM::MethodDescriptor::GetDefToken() const
{
	return mSelf;
}

RTM::MethodTable* RTM::MethodDescriptor::GetOwningTable() const
{
	return mOwningTable;
}

RTO::StringObject* RTM::MethodDescriptor::GetName() const
{
	return mManagedName;
}

bool RTM::MethodDescriptor::IsStatic() const
{
	return mColdMD->IsStatic();
}

bool RTM::MethodDescriptor::IsVirtual() const
{
	return mColdMD->IsVirtual();
}

bool RTM::MethodDescriptor::IsOverride() const
{
	return mColdMD->IsOverride();
}

bool RTM::MethodDescriptor::IsFinal() const
{
	return mColdMD->IsFinal();
}

bool RTM::MethodDescriptor::IsGeneric() const
{
	return mColdMD->IsGeneric();
}

void RTM::MethodDescriptor::SetCallingConvention(RTP::PlatformCallingConvention* callingConv)
{
	mCallingConv = callingConv;
}

RTP::PlatformCallingConvention* RTM::MethodDescriptor::GetCallingConvention() const
{
	return mCallingConv;
}

RTM::TypeDescriptor* RTM::MethodArgumentDescriptor::GetType()
{
	return mType;
}

RTO::StringObject* RTM::MethodArgumentDescriptor::GetName()
{
	return mManagedName;
}

RTM::TypeDescriptor* RTM::MethodLocalVariableDescriptor::GetType()
{
	return mType;
}

RTO::StringObject* RTM::MethodLocalVariableDescriptor::GetName()
{
	return mManagedName;
}

RT::ObservableArray<RTM::MethodArgumentDescriptor> RTM::MethodSignatureDescriptor::GetArguments() const
{
	return { mArguments, mColdMD->ArgumentCount };
}

RTM::TypeDescriptor* RTM::MethodSignatureDescriptor::GetReturnType() const
{
	return mReturnType;
}
