#include "FieldTable.h"
#include "..\Exception\RuntimeException.h"

RT::ObservableArray<RTM::FieldDescriptor> RTM::FieldTable::GetFields() const
{
    return { mFields, mFieldCount };
}

RTM::FieldDescriptor* RTM::FieldTable::GetFieldBy(MDToken fieldDefToken) const
{
    if (fieldDefToken == NullToken ||
        fieldDefToken < mBaseToken ||
        (fieldDefToken - mBaseToken > mFieldCount))
        THROW("Token out of ranges");

    return &mFields[fieldDefToken - mBaseToken];
}

RTM::FieldsLayout* RTM::FieldTable::GetLayout() const
{
    return mLayout;
}
