#include "MethodDescriptor.h"

RT::UInt8 RTM::MethodDescriptor::GetAccessbility() const
{
	return mColdMD->Flags.Accessibility;
}

RTME::MethodSignatureMD* RTM::MethodDescriptor::GetSignature() const
{
	return &mColdMD->Signature;
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
	return mColdMD->Flags.IsInstance;
}

bool RTM::MethodDescriptor::IsVirtual() const
{
	return mColdMD->Flags.IsVirtual;
}

bool RTM::MethodDescriptor::IsOverride() const
{
	return mColdMD->Flags.IsOverride;
}

bool RTM::MethodDescriptor::IsFinal() const
{
	return mColdMD->Flags.IsFinal;
}

bool RTM::MethodDescriptor::IsGeneric() const
{
	return mColdMD->Flags.IsGeneric;
}