#include "MethodDescriptor.h"

RTM::MethodMD* RTM::MethodDescriptor::GetMetadata() const
{
	return mColdMD;
}

RT::UInt8 RTM::MethodDescriptor::GetAccessbility() const
{
	return mColdMD->Flags.Accessibility;
}

RTM::MethodSignatureMD* RTM::MethodDescriptor::GetSignature() const
{
	return &mColdMD->SignatureMD;
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