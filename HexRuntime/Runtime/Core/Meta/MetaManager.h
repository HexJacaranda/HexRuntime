#pragma once
#include "..\..\RuntimeAlias.h"
#include "..\Object\StringObject.h"
#include "MDRecords.h"
#include "MDImporter.h"
#include "MethodDescriptor.h"
#include "FieldDescriptor.h"
#include "TypeDescriptor.h"

namespace RTM
{
	/// <summary>
	/// Manager that responsible for metadata management
	/// </summary>
	class MetaManager
	{
		RTME::MDImporter* mImporters;
	public:
		static TypeDescriptor* GetTypeFromToken(MDToken typeReference);
		static RTO::StringObject* GetStringFromToken(MDToken stringToken);
		static MethodDescriptor* GetMethodFromToken(MDToken methodReference);
		static FieldDescriptor* GetFieldFromToken(MDToken fieldReference);
	};
}