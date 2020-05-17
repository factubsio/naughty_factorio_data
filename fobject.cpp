#include "fobject.hpp"

FValue FValue::nil;

FObject FObject::nil(false);

const FObject& FValue::obj() const {
    const FObject*const* obj = as<FObject*>();
    if (obj)
        return **obj;
    else
        return FObject::nil;
}


FObject &FValue::obj()
{
    return const_cast<FObject&>(std::as_const(*this).obj());
}

