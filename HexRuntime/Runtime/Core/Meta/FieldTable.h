#pragma once
#include "..\..\RuntimeAlias.h"
#include "..\..\ObservableArray.h"
#include "FieldDescriptor.h"

namespace RTM
{
	struct LayoutSlot
	{
		Int32 Offset;
	};

	struct FieldsLayout
	{
		Int32 Size;
		Int8 Align;
		Int32 SlotCount;
		LayoutSlot* Slots;
	};

	class FieldTable
	{
		friend class MetaManager;

		FieldsLayout* mLayout;
		Int32 mFieldCount;
		FieldDescriptor* mFields;
		MDToken mBaseToken;
	public:
		ObservableArray<FieldDescriptor> GetFields()const;
		FieldDescriptor* GetFieldBy(MDToken fieldDefToken)const;
		FieldsLayout* GetLayout()const;
	};
}