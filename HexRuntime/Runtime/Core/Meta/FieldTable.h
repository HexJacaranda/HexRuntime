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

		FieldsLayout* Layout;
		Int32 FieldCount;
		FieldDescriptor** Fields;
	public:
		ObservableArray<FieldDescriptor*> GetFields()const;
		FieldsLayout* GetLayout()const;
	};
}