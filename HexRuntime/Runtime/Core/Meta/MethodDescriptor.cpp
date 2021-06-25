#include "MethodDescriptor.h"

RT::UInt8 RTM::MethodDescriptor::GetAccessbility() const
{
	return mColdMD->Accessibility;
}

RTM::MethodSignature* RTM::MethodDescriptor::GetSignature() const
{
	return nullptr;
}

RT::ObservableArray<RTME::ArgumentMD> RTM::MethodDescriptor::GetArguments() const
{
	return { mArguments, mColdMD->Signature.ArgumentCount };
}

RT::ObservableArray<RTME::LocalVariableMD> RTM::MethodDescriptor::GetLocalVariables() const
{
	return { mColdMD->ILCodeMD.LocalVariables,  mColdMD->ILCodeMD.LocalVariableCount };
}

RTME::ILMD* RTM::MethodDescriptor::GetIL() const
{
	return &mColdMD->ILCodeMD;
}

bool RTM::MethodDescriptor::IsInstance() const
{
	return mColdMD->IsInstance();
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