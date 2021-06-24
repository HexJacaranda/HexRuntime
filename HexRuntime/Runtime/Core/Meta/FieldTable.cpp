#include "FieldTable.h"

RT::ObservableArray<RTM::FieldDescriptor*> RTM::FieldTable::GetFields() const
{
    return { Fields, FieldCount };
}

RTM::FieldsLayout* RTM::FieldTable::GetLayout() const
{
    return Layout;
}
