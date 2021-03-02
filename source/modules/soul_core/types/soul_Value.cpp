/*
    _____ _____ _____ __
   |   __|     |  |  |  |      The SOUL language
   |__   |  |  |  |  |  |__    Copyright (c) 2019 - ROLI Ltd.
   |_____|_____|_____|_____|

   The code in this file is provided under the terms of the ISC license:

   Permission to use, copy, modify, and/or distribute this software for any purpose
   with or without fee is hereby granted, provided that the above copyright notice and
   this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD
   TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN
   NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
   DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
   IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
   CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#if ! SOUL_INSIDE_CORE_CPP
 #error "Don't add this cpp file to your build, it gets included indirectly by soul_core.cpp"
#endif

namespace soul
{

//==============================================================================
struct Value::PackedData
{
    PackedData (const Type& t, uint8_t* d, size_t s) : type (t), data (d), size (s)
    {
        SOUL_ASSERT (type.isValid() && ! type.isVoid());
    }

    void clear() const          { memset (data, 0, size); }

    void print (ValuePrinter& p)
    {
        if (type.isPrimitive())
        {
            if (type.isInteger32())    return p.printInt32 (getAs<int32_t>());
            if (type.isInteger64())    return p.printInt64 (getAs<int64_t>());
            if (type.isBool())         return p.printBool (getAs<uint8_t>() != 0);
            if (type.isFloat32())      return p.printFloat32 (getAs<float>());
            if (type.isFloat64())      return p.printFloat64 (getAs<double>());

            if (type.isComplex32() && ! isZero())
            {
                auto v = getAsComplex32();
                p.beginArrayMembers (type);
                p.printFloat32 (v.real());
                p.printArrayMemberSeparator();
                p.printFloat32 (v.imag());
                p.endStructMembers();
                return;
            }

            if (type.isComplex64() && ! isZero())
            {
                auto v = getAsComplex64();
                p.beginArrayMembers (type);
                p.printFloat64 (v.real());
                p.printArrayMemberSeparator();
                p.printFloat64 (v.imag());
                p.endStructMembers();
                return;
            }
        }

        if (type.isBoundedInt())     return p.printInt32 (getAs<Type::BoundedIntSize>());
        if (type.isStringLiteral())  return p.printStringLiteral (getAs<StringDictionary::Handle>());
        if (type.isUnsizedArray())   return p.printUnsizedArrayContent (type, getAs<void*>());

        if (! isZero())
        {
            if (type.isArrayOrVector())
            {
                p.beginArrayMembers (type);
                bool isFirst = true;

                for (ArrayIterator i (*this); i.next();)
                {
                    if (isFirst)
                        isFirst = false;
                    else
                        p.printArrayMemberSeparator();

                    i.get().print (p);
                }

                return p.endArrayMembers();
            }

            if (type.isStruct() && ! type.getStructRef().empty())
            {
                p.beginStructMembers (type);
                bool isFirst = true;

                for (StructIterator i (*this); i.next();)
                {
                    if (isFirst)
                        isFirst = false;
                    else
                        p.printStructMemberSeparator();

                    i.get().print (p);
                }

                return p.endStructMembers();
            }
        }

        return p.printZeroInitialiser (type);
    }

    bool isZero() const
    {
        if (type.isVoid())
            return true;

        for (size_t i = 0; i < size; ++i)
            if (data[i] != 0)
                return false;

        return true;
    }

    bool equals (const PackedData& other) const
    {
        if (! type.isIdentical (other.type))
            return false;

        return memcmp (data, other.data, size) == 0;
    }

    bool getAsBool() const
    {
        SOUL_ASSERT (type.isPrimitive() || type.isBoundedInt() || type.isVectorOfSize1());

        if (type.isBool())           return getAs<uint8_t>() != 0;
        if (type.isInteger())        return getAsInt64() != 0;
        if (type.isFloatingPoint())  return getAsDouble() != 0;

        SOUL_ASSERT_FALSE;
        return false;
    }

    double getAsDouble() const
    {
        SOUL_ASSERT (type.isPrimitive() || type.isVectorOfSize1());

        if (type.isFloat32())        return getAs<float>();
        if (type.isFloat64())        return getAs<double>();
        if (type.isBool())           return getAs<uint8_t>() != 0 ? 1.0 : 0.0;
        if (type.isInteger())        return static_cast<double> (getAsInt64());

        SOUL_ASSERT_FALSE;
        return {};
    }

    std::complex<float> getAsComplex32() const
    {
        if (type.isComplex32())
            return getAs<std::complex<float>>();

        return static_cast<std::complex<float>> (getAsComplex64());
    }

    std::complex<double> getAsComplex64() const
    {
        if (type.isComplex64())  return getAs<std::complex<double>>();
        if (type.isComplex32())  return getAs<std::complex<float>>();

        return { getAsDouble(), 0.0 };
    }

    int64_t getAsInt64() const
    {
        SOUL_ASSERT (type.isPrimitive() || type.isBoundedInt() || type.isVectorOfSize1());

        if (type.isInteger32())      return getAs<int32_t>();
        if (type.isInteger64())      return getAs<int64_t>();
        if (type.isBool())           return getAs<uint8_t>() != 0 ? 1 : 0;
        if (type.isFloatingPoint())  return static_cast<int64_t> (getAsDouble());

        SOUL_ASSERT_FALSE;
        return {};
    }

    StringDictionary::Handle getStringLiteral() const
    {
        SOUL_ASSERT (type.isStringLiteral());
        return getAs<StringDictionary::Handle>();
    }

    choc::buffer::InterleavedView<float> getAsChannelSet32() const
    {
        auto elementType = type.getElementType();
        SOUL_ASSERT (elementType.isFloat32());
        return choc::buffer::createInterleavedView (reinterpret_cast<float*> (data),
                                                    elementType.getVectorSize(), type.getArraySize());
    }

    choc::buffer::InterleavedView<double> getAsChannelSet64() const
    {
        auto elementType = type.getElementType();
        SOUL_ASSERT (elementType.isFloat64());
        return choc::buffer::createInterleavedView (reinterpret_cast<double*> (data),
                                                    elementType.getVectorSize(), type.getArraySize());
    }

    void setFrom (const PackedData& other) const
    {
        if (other.isZero())
            return clear();

        if (type.isPrimitive())
        {
            if (type.isInteger32())     return setAs (other.type.isFloatingPoint() ? (int32_t) other.getAsDouble() : (int32_t) other.getAsInt64());
            if (type.isInteger64())     return setAs (other.type.isFloatingPoint() ? (int64_t) other.getAsDouble() : (int64_t) other.getAsInt64());
            if (type.isFloat32())       return setAs ((float) other.getAsDouble());
            if (type.isFloat64())       return setAs (other.getAsDouble());
            if (type.isBool())          return setAs (other.getAsBool() ? (uint8_t) 1 : (uint8_t) 0);
            if (type.isComplex32())     return setAs (other.getAsComplex32());
            if (type.isComplex64())     return setAs (other.getAsComplex64());

            SOUL_ASSERT_FALSE;
        }

        if (type.isBoundedInt())        return setAs (static_cast<Type::BoundedIntSize> (wrapOrClampToLegalValue (type, other.getAsInt64())));
        if (type.isUnsizedArray())      return setAs (other.getAs<ConstantTable::Handle>());
        if (type.isStringLiteral())     return setAs (other.getAs<StringDictionary::Handle>());

        if (type.isArrayOrVector())
        {
            if (! (other.type.isArrayOrVector() && other.type.getArrayOrVectorSize() == type.getArrayOrVectorSize()))
            {
                for (ArrayIterator dst (*this); dst.next();)
                    dst.get().setFrom (other);

                return;
            }

            ArrayIterator dst (*this), src (other);

            for (;;)
            {
                bool ok1 = dst.next();
                bool ok2 = src.next();
                ignoreUnused (ok2);
                SOUL_ASSERT (ok1 == ok2);

                if (! ok1)
                    return;

                dst.get().setFrom (src.get());
            }
        }

        if (type.isStruct())
        {
            StructIterator dst (*this), src (other);

            for (;;)
            {
                bool ok1 = dst.next();
                bool ok2 = src.next();
                ignoreUnused (ok2);
                SOUL_ASSERT (ok1 == ok2);

                if (! ok1)
                    return;

                dst.get().setFrom (src.get());
            }
        }
    }

    void setFrom (choc::span<Value> values) const
    {
        if (values.size() == 0)
            return clear();

        if (type.isArrayOrVector() && ! type.isUnsizedArray())
        {
            if (values.size() == 1)
            {
                auto src = values.front().getData();

                for (ArrayIterator dst (*this); dst.next();)
                    dst.get().setFrom (src);

                return;
            }

            SOUL_ASSERT (values.size() == type.getArrayOrVectorSize());
            auto src = values.begin();

            for (ArrayIterator dst (*this); dst.next();)
            {
                dst.get().setFrom (src->getData());
                ++src;
            }

            return;
        }

        if (type.isStruct())
        {
            SOUL_ASSERT (values.size() == type.getStructRef().getNumMembers());
            auto src = values.begin();

            for (StructIterator dst (*this); dst.next();)
            {
                dst.get().setFrom (src->getData());
                ++src;
            }

            return;
        }

        SOUL_ASSERT_FALSE;
    }

    void negate() const
    {
        if (type.isArrayOrVector())
        {
            for (ArrayIterator i (*this); i.next();)
                i.get().negate();

            return;
        }

        if (type.isPrimitive())
        {
            if (type.isInteger32())      return negateAs<int32_t>();
            if (type.isInteger64())      return negateAs<int64_t>();
            if (type.isFloat32())        return negateAs<float>();
            if (type.isFloat64())        return negateAs<double>();
            if (type.isComplex32())      return negateAs<std::complex<float>>();
            if (type.isComplex64())      return negateAs<std::complex<double>>();
        }

        SOUL_ASSERT_FALSE;
    }

    template <typename Primitive>
    Primitive getAs() const                 { SOUL_ASSERT (size == sizeof (Primitive)); return readUnaligned<Primitive> (data); }

    template <typename Primitive>
    void setAs (Primitive newValue) const   { SOUL_ASSERT (size == sizeof (Primitive)); writeUnaligned (data, newValue); }

    template <typename Primitive>
    void negateAs() const                   { setAs (-getAs<Primitive>()); }

    void convertAllHandlesToPointers (ConstantTable& constantTable)
    {
        if (type.isUnsizedArray())
        {
            auto source = constantTable.getValueForHandle (getAs<ConstantTable::Handle>());
            SOUL_ASSERT (source != nullptr);
            setAs<void*> (source->getPackedData());
        }
        else if (type.isArrayOrVector())
        {
            for (ArrayIterator i (*this); i.next();)
                i.get().convertAllHandlesToPointers (constantTable);
        }
        else if (type.isStruct())
        {
            for (StructIterator i (*this); i.next();)
                i.get().convertAllHandlesToPointers (constantTable);
        }
    }

    struct ArrayIterator
    {
        ArrayIterator (const PackedData& p)
            : elementType (p.type.getElementType()), element (p.data),
              numElements (p.type.getArrayOrVectorSize()), elementSize ((size_t) elementType.getPackedSizeInBytes())
        {}

        bool next()
        {
            if (index >= numElements)
                return false;

            if (index > 0)
                element += elementSize;

            ++index;
            return true;
        }

        PackedData get() const      { return PackedData (elementType, element, elementSize); }

        Type elementType;
        uint8_t* element;
        size_t index = 0;
        const size_t numElements, elementSize;
    };

    struct StructIterator
    {
        StructIterator (const PackedData& p)
            : structure (p.type.getStructRef()), member (p.data),
              numMembers (structure.getNumMembers())
        {}

        bool next()
        {
            if (index >= numMembers)
                return false;

            member += memberSize;
            memberSize = (size_t) structure.getMemberType (index++).getPackedSizeInBytes();
            return true;
        }

        PackedData get() const      { return PackedData (structure.getMemberType (index - 1), member, memberSize); }

        const Structure& structure;
        uint8_t* member;
        size_t memberSize = 0, index = 0;
        const size_t numMembers;
    };

    static int64_t wrapOrClampToLegalValue (const Type& type, int64_t value)
    {
        if (type.isWrapped())
        {
            auto size = (int64_t) type.getBoundedIntLimit();
            value %= size;
            return value < 0 ? value + size : value;
        }

        if (type.isClamped())
        {
            auto size = (int64_t) type.getBoundedIntLimit();
            return value < 0 ? 0 : (value >= size ? size - 1 : value);
        }

        SOUL_ASSERT_FALSE;
        return value;
    }

    const Type& type;
    uint8_t* const data;
    const size_t size;
};

//==============================================================================
Value::Value() = default;
Value::~Value() = default;
Value::Value (const Value&) = default;
Value& Value::operator= (const Value&) = default;
Value::Value (Value&&) = default;
Value& Value::operator= (Value&& other) = default;

Value::Value (Type t)  : type (std::move (t)), allocatedData ((size_t) type.getPackedSizeInBytes()) {}

Value::Value (Type t, const void* sourceData)   : Value (std::move (t))
{
    memcpy (getPackedData(), sourceData, getPackedDataSize());
}

Value::Value (int32_t  v)  : Value (Type (PrimitiveType::int32))     { getData().setAs (v); }
Value::Value (int64_t  v)  : Value (Type (PrimitiveType::int64))     { getData().setAs (v); }
Value::Value (float    v)  : Value (Type (PrimitiveType::float32))   { getData().setAs (v); }
Value::Value (double   v)  : Value (Type (PrimitiveType::float64))   { getData().setAs (v); }
Value::Value (bool     v)  : Value (Type (PrimitiveType::bool_))     { getData().setAs (v ? (uint8_t) 1 : (uint8_t) 0); }

Value::Value (std::complex<float> v)  : Value (Type (PrimitiveType::complex32))   { getData().setAs (v); }
Value::Value (std::complex<double> v) : Value (Type (PrimitiveType::complex64))   { getData().setAs (v); }

Value Value::createArrayOrVector (Type t, choc::span<Value> elements)
{
    Value v (std::move (t));
    v.getData().setFrom (elements);
    return v;
}

Value Value::createStruct (Structure& s, choc::span<Value> members)
{
    Value v (Type::createStruct (s));
    v.getData().setFrom (members);
    return v;
}

Value Value::createUnsizedArray (const Type& elementType, ConstantTable::Handle h)
{
    SOUL_ASSERT (! elementType.isUnsizedArray());  // this may need to be removed at some point, but is here as a useful sanity-check
    Value v (elementType.createUnsizedArray());
    v.getData().setAs (h);
    return v;
}

Value Value::createFloatVectorArray (choc::buffer::InterleavedView<float> data)
{
    Value v (Type::createVector (PrimitiveType::float32, data.getNumChannels()).createArray (data.getNumFrames()));
    copy (v.getAsChannelSet32(), data);
    return v;
}

Value Value::createFloatVectorArray (choc::buffer::ChannelArrayView<float> data)
{
    Value v (Type::createVector (PrimitiveType::float32, data.getNumChannels()).createArray (data.getNumFrames()));
    copy (v.getAsChannelSet32(), data);
    return v;
}

choc::buffer::InterleavedView<float>  Value::getAsChannelSet32() const   { return getData().getAsChannelSet32(); }
choc::buffer::InterleavedView<double> Value::getAsChannelSet64() const   { return getData().getAsChannelSet64(); }

Value Value::zeroInitialiser (Type t)
{
    SOUL_ASSERT (t.isValid() && ! t.isVoid());
    return Value (std::move (t));
}

Value Value::createStringLiteral (StringDictionary::Handle h)
{
    Value v (Type::createStringLiteral());
    v.getData().setAs (h);
    return v;
}

Value Value::createFromRawData (Type type, const void* sourceData, size_t dataSize)
{
    ignoreUnused (dataSize);

    Value v (std::move (type));
    SOUL_ASSERT (dataSize == v.getPackedDataSize());
    memcpy (v.getPackedData(), sourceData, v.getPackedDataSize());
    return v;
}

bool Value::getAsBool() const                               { return getData().getAsBool(); }
float Value::getAsFloat() const                             { return (float) getAsDouble(); }
double Value::getAsDouble() const                           { return getData().getAsDouble(); }
int32_t Value::getAsInt32() const                           { return (int32_t) getAsInt64(); }
int64_t Value::getAsInt64() const                           { return getData().getAsInt64(); }
StringDictionary::Handle Value::getStringLiteral() const    { return getData().getAs<StringDictionary::Handle>(); }
ConstantTable::Handle Value::getUnsizedArrayContent() const { return getData().getAs<ConstantTable::Handle>(); }
std::complex<float>  Value::getAsComplex32() const          { return getData().getAsComplex32(); }
std::complex<double> Value::getAsComplex64() const          { return getData().getAsComplex64(); }

Value::operator float() const                               { return getAsFloat(); }
Value::operator double() const                              { return getAsDouble(); }
Value::operator int32_t() const                             { return getAsInt32(); }
Value::operator int64_t() const                             { return getAsInt64(); }
Value::operator bool() const                                { return getAsBool(); }

bool Value::isValid() const                                 { return type.isValid(); }
bool Value::isZero() const                                  { return ! isValid() || ! allocatedData.isAllocated() || getData().isZero(); }

const Type& Value::getType() const                          { return type; }
Type& Value::getMutableType()                               { return type; }

Value::PackedData Value::getData() const
{
    SOUL_ASSERT (isValid());
    return PackedData (type, allocatedData.data(), allocatedData.size());
}

void Value::print (ValuePrinter& p) const
{
    if (isZero() && (type.isArray() || type.isVector()))
        p.printZeroInitialiser (type);
    else
        getData().print (p);
}

std::string Value::getDescription (const StringDictionary* dictionary) const
{
    struct DefaultPrinter  : public ValuePrinter
    {
        void print (std::string_view s) override  { out << s; }
        std::ostringstream out;
    };

    DefaultPrinter p;
    p.dictionary = dictionary;
    print (p);
    return p.out.str();
}

Value Value::getSubElement (const SubElementPath& path) const
{
    auto typeAndOffset = path.getElement (type);
    return Value (std::move (typeAndOffset.type), allocatedData.data() + typeAndOffset.offset);
}

void Value::modifySubElementInPlace (const SubElementPath& path, const void* newData)
{
    auto typeAndOffset = path.getElement (type);
    memcpy (allocatedData.data() + typeAndOffset.offset, newData, (size_t) typeAndOffset.type.getPackedSizeInBytes());
}

void Value::modifySubElementInPlace (const SubElementPath& path, const Value& newValue)
{
    auto typeAndOffset = path.getElement (type);
    SOUL_ASSERT (typeAndOffset.type.hasIdenticalLayout (newValue.getType()));
    memcpy (allocatedData.data() + typeAndOffset.offset, newValue.getPackedData(), newValue.getPackedDataSize());
}


Value Value::getSlice (size_t start, size_t end) const
{
    if (type.isArrayOrVector())
    {
        SOUL_ASSERT (! type.isUnsizedArray());
        SOUL_ASSERT (type.isValidArrayOrVectorRange (start, end));

        auto elementType = type.getElementType();
        auto elementSize = elementType.getPackedSizeInBytes();

        return Value (type.createCopyWithNewArraySize (end - start), allocatedData.data() + elementSize * start);
    }

    SOUL_ASSERT_FALSE;
    return {};
}

void Value::copyValue (const Value& source)
{
    if (type.isIdentical (source.type))
    {
        memcpy (allocatedData.data(), source.allocatedData.data(), allocatedData.size());
        return;
    }

    SOUL_ASSERT_FALSE;
}

bool Value::canNegate() const
{
    return type.isFloatingPoint() || type.isInteger() || type.isComplex();
}

Value Value::negated() const
{
    Value v (*this);
    v.getData().negate();
    return v;
}

bool Value::operator== (const Value& other) const
{
    if (! type.isValid())
        return ! other.type.isValid();

    return type.isIdentical (other.type) && getData().equals (other.getData());
}

bool Value::operator!= (const Value& other) const       { return ! operator== (other); }

Value Value::cloneWithEquivalentType (Type newType) const
{
    SOUL_ASSERT (newType.hasIdenticalLayout (type));

    if (isZero())
        return Value (std::move (newType));

    return Value (std::move (newType), getPackedData());
}

void Value::clear()
{
    getData().clear();
}

Value Value::tryCastToType (const Type& destType) const
{
    if (destType.isIdentical (type))
        return *this;

    if (! TypeRules::canCastTo (destType, type))
        return {};

    if (destType.isUnsizedArray()
          && ! destType.removeConstIfPresent().isIdentical (type))
        return {};

    Value v (destType);
    v.getData().setFrom (getData());
    return v;
}

Value Value::tryCastToType (const Type& destType, CompileMessage& errorMessage) const
{
    auto result = tryCastToType (destType);

    if (! result.isValid())
    {
        if (type.isPrimitive())
            errorMessage = Errors::cannotCastValue (getDescription(), type.getDescription(), destType.getDescription());
        else
            errorMessage = Errors::cannotCastBetween (type.getDescription(), destType.getDescription());
    }

    return result;
}

Value Value::castToTypeExpectingSuccess (const Type& destType) const
{
    auto result = tryCastToType (destType);
    SOUL_ASSERT (result.isValid());
    return result;
}

void Value::convertAllHandlesToPointers (ConstantTable& constantTable)
{
    getData().convertAllHandlesToPointers (constantTable);
}

void Value::modifyArraySizeInPlace (size_t newSize)
{
    SOUL_ASSERT (type.isArray());
    auto newType = type.createCopyWithNewArraySize ((Type::ArraySize) newSize);
    SOUL_ASSERT (newType.getPackedSizeInBytes() <= allocatedData.size());
    type = std::move (newType);
}

//==============================================================================
Value Value::fromExternalValue (const Type& targetType, const choc::value::ValueView& sourceValue,
                                ConstantTable& constantTable, StringDictionary& stringDictionary)
{
    // put all conversion state into a single object to avoid lambdas needing to capture more than a single pointer to the state
    struct ConversionState
    {
        ConstantTable& constants;
        StringDictionary& dictionary;

        static Value castOrThrow (const Type& type, Value&& v)
        {
            if (v.getType().isIdentical (type))
                return std::move (v);

            CompileMessage errorMessage;
            auto result = v.tryCastToType (type, errorMessage);

            if (! result.isValid())
                throwError (errorMessage);

            return result;
        }

        Value convert (const Type& targetType, const choc::value::ValueView& source)
        {
            if (source.isInt32())    return castOrThrow (targetType, Value::createInt32 (source.getInt32()));
            if (source.isInt64())    return castOrThrow (targetType, Value::createInt64 (source.getInt64()));
            if (source.isFloat32())  return castOrThrow (targetType, Value (source.getFloat32()));
            if (source.isFloat64())  return castOrThrow (targetType, Value (source.getFloat64()));
            if (source.isBool())     return castOrThrow (targetType, Value (source.getBool()));
            if (source.isString())   return castOrThrow (targetType, Value::createStringLiteral (dictionary.getHandleForString (std::string (source.getString()))));

            if (source.isVector())
            {
                auto size = source.size();

                if (size == 1 && targetType.isPrimitive())
                    return convert (targetType, source[0]);

                if (! targetType.isVector())
                    throwError (Errors::cannotCastBetween ("vector", targetType.getDescription()));

                auto elementType = targetType.getVectorElementType();
                auto result = Value::zeroInitialiser (Type::createVector (elementType, size));

                for (uint32_t i = 0; i < size; ++i)
                    result.modifySubElementInPlace (i, convert (elementType, source[i]));

                return castOrThrow (targetType, std::move (result));
            }

            if (source.isArray())
            {
                auto size = source.size();

                if (targetType.isUnsizedArray())
                {
                    auto elementType = targetType.getElementType();
                    auto fixedArray = convert (elementType.createArray (size), source);
                    return Value::createUnsizedArray (elementType, constants.getHandleForValue (std::move (fixedArray)));
                }

                if (! (targetType.isArray() && size == targetType.getArraySize()))
                    throwError (Errors::cannotCastBetween ("array[" + std::to_string (size) + "]", targetType.getDescription()));

                auto elementType = targetType.getArrayElementType();
                auto result = Value::zeroInitialiser (elementType.createArray (size));

                for (uint32_t i = 0; i < size; ++i)
                    result.modifySubElementInPlace (i, convert (elementType, source[i]));

                return castOrThrow (targetType, std::move (result));
            }

            if (source.isObject())
            {
                if (targetType.isStruct())
                {
                    auto& targetStruct = targetType.getStructRef();
                    auto numMembers = source.size();

                    if (targetStruct.getMembers().size() == numMembers)
                    {
                        auto result = Value::zeroInitialiser (targetType);

                        for (uint32_t j = 0; j < targetStruct.getMembers().size(); ++j)
                        {
                            auto& m = targetStruct.getMembers()[j];
                            bool found = false;

                            for (uint32_t i = 0; i < numMembers; ++i)
                            {
                                auto sourceMember = source.getObjectMemberAt (i);

                                if (m.name == sourceMember.name)
                                {
                                    result.modifySubElementInPlace (j, convert (m.type, sourceMember.value));
                                    found = true;
                                    break;
                                }
                            }

                            if (! found)
                                throwError (Errors::unknownMemberInStruct (m.name, targetStruct.getName()));
                        }

                        return result;
                    }
                }

                throwError (Errors::cannotCastBetween ("struct " + std::string (source.getObjectClassName()),
                                                       targetType.getDescription()));
            }

            return {};
        }
    };

    ConversionState c { constantTable, stringDictionary };
    return c.convert (targetType, sourceValue);
}

choc::value::Value Value::toExternalValue (const ConstantTable& constantTable, const StringDictionary& stringDictionary) const
{
    // put all conversion state into a single object to avoid lambdas needing to capture more than a single pointer to the state
    struct ConversionState
    {
        const ConstantTable& constants;
        const StringDictionary& dictionary;

        choc::value::Value convert (const Value& source)
        {
            const auto& sourceType = source.getType();

            if (sourceType.isPrimitive())
            {
                if (sourceType.isInteger32())   return choc::value::createInt32 (source.getAsInt32());
                if (sourceType.isInteger64())   return choc::value::createInt64 (source.getAsInt64());
                if (sourceType.isFloat32())     return choc::value::createFloat32 (source.getAsFloat());
                if (sourceType.isFloat64())     return choc::value::createFloat64 (source.getAsDouble());
                if (sourceType.isBool())        return choc::value::createBool (source.getAsBool());
            }

            if (sourceType.isStringLiteral())
                return choc::value::createString (dictionary.getStringForHandle (source.getStringLiteral()));

            if (sourceType.isVector())
            {
                auto size = static_cast<uint32_t> (sourceType.getVectorSize());
                auto elementType = sourceType.getElementType();

                if (elementType.isInteger32())   return choc::value::createVector (size, [&] (uint32_t index)  { return source.getSubElement (index).getAsInt32(); });
                if (elementType.isInteger64())   return choc::value::createVector (size, [&] (uint32_t index)  { return source.getSubElement (index).getAsInt64(); });
                if (elementType.isFloat32())     return choc::value::createVector (size, [&] (uint32_t index)  { return source.getSubElement (index).getAsFloat(); });
                if (elementType.isFloat64())     return choc::value::createVector (size, [&] (uint32_t index)  { return source.getSubElement (index).getAsDouble(); });
                if (elementType.isBool())        return choc::value::createVector (size, [&] (uint32_t index)  { return source.getSubElement (index).getAsBool(); });
            }

            if (sourceType.isUnsizedArray())
            {
                auto target = constants.getValueForHandle (source.getUnsizedArrayContent());
                SOUL_ASSERT (target != nullptr);
                return convert (*target);
            }

            if (sourceType.isArray())
                return choc::value::createArray (static_cast<uint32_t> (sourceType.getArraySize()),
                                                 [&] (uint32_t i) { return convert (source.getSubElement (i)); });

            if (sourceType.isStruct())
            {
                auto& s = sourceType.getStructRef();
                auto o = choc::value::createObject (s.getName());
                size_t i = 0;

                for (auto& m : s.getMembers())
                    o.addMember (m.name, convert (source.getSubElement (i++)));

                return o;
            }

            SOUL_ASSERT_FALSE;
            return {};
        }
    };

    ConversionState c { constantTable, stringDictionary };
    return c.convert (*this);
}

//==============================================================================
ValuePrinter::ValuePrinter() = default;
ValuePrinter::~ValuePrinter() = default;

void ValuePrinter::printZeroInitialiser (const Type&)    { print ("{}"); }
void ValuePrinter::printBool (bool b)                    { print (b ? "true" : "false"); }
void ValuePrinter::printInt32 (int32_t v)                { print (std::to_string (v)); }
void ValuePrinter::printInt64 (int64_t v)                { print (std::to_string (v) + "L"); }

void ValuePrinter::printFloat32 (float value)
{
    if (value == 0)             return print ("0");
    if (std::isnan (value))     return print ("_nan32");
    if (std::isinf (value))     return print (value > 0 ? "_inf32" : "_ninf32");

    return print (choc::text::floatToString (value) + "f");
}

void ValuePrinter::printFloat64 (double value)
{
    if (value == 0)             return print ("0");
    if (std::isnan (value))     return print ("_nan64");
    if (std::isinf (value))     return print (value > 0 ? "_inf64" : "_ninf64");

    return print (choc::text::floatToString (value));
}

void ValuePrinter::beginStructMembers (const Type&)       { print ("{ "); }
void ValuePrinter::printStructMemberSeparator()           { print (", "); }
void ValuePrinter::endStructMembers()                     { print (" }"); }
void ValuePrinter::beginArrayMembers (const Type&)        { print ("{ "); }
void ValuePrinter::printArrayMemberSeparator()            { print (", "); }
void ValuePrinter::endArrayMembers()                      { print (" }"); }
void ValuePrinter::beginVectorMembers (const Type&)       { print ("{ "); }
void ValuePrinter::printVectorMemberSeparator()           { print (", "); }
void ValuePrinter::endVectorMembers()                     { print (" }"); }

void ValuePrinter::printStringLiteral (StringDictionary::Handle h)
{
    print (dictionary != nullptr ? choc::text::addDoubleQuotes (std::string (dictionary->getStringForHandle (h)))
                                 : std::to_string (h.handle));
}

void ValuePrinter::printUnsizedArrayContent (const Type&, const void* pointer)
{
    if (pointer == nullptr)
        return print ("{}");

    ConstantTable::Handle handle;
    writeUnaligned (std::addressof (handle), pointer);
    print ("0x" + choc::text::createHexString (handle));
}


} // namespace soul
