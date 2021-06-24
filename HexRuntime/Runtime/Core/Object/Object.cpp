#include "Object.h"
#include "ArrayObject.h"
#include "ObjectStorage.h"

inline RTM::Type* RTO::Object::GetType()const
{
    return mType;
}

inline RT::UInt32 RTO::Object::GetObjectSize() const
{
    if (mType->IsArray())
    {
        ArrayObject* array = (ArrayObject*)this;
        return array->GetCount() * array->GetElementType()->GetSize();
    }
    return GetType()->GetSize();
}

inline RTO::ObjectStorage* RTO::Object::GetStorage() const
{
    return (RTO::ObjectStorage*)this - 1;
}
