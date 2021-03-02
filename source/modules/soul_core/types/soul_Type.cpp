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

PrimitiveType::PrimitiveType (const PrimitiveType&) = default;
PrimitiveType& PrimitiveType::operator= (const PrimitiveType&) = default;

bool PrimitiveType::operator== (const PrimitiveType& other) const      { return type == other.type; }
bool PrimitiveType::operator!= (const PrimitiveType& other) const      { return ! operator== (other); }

bool PrimitiveType::isValid() const                 { return type != Primitive::invalid; }
bool PrimitiveType::isVoid() const                  { return type == Primitive::void_; }
bool PrimitiveType::isFloat32() const               { return type == Primitive::float32; }
bool PrimitiveType::isFloat64() const               { return type == Primitive::float64; }
bool PrimitiveType::isFloatingPoint() const         { return isFloat32() || isFloat64(); }
bool PrimitiveType::isFixed() const                 { return type == Primitive::fixed; }
bool PrimitiveType::isComplex() const               { return type == Primitive::complex32 || type == Primitive::complex64; }
bool PrimitiveType::isComplex32() const             { return type == Primitive::complex32; }
bool PrimitiveType::isComplex64() const             { return type == Primitive::complex64; }
bool PrimitiveType::isInteger() const               { return type == Primitive::int32 || type == Primitive::int64; }
bool PrimitiveType::isInteger32() const             { return type == Primitive::int32; }
bool PrimitiveType::isInteger64() const             { return type == Primitive::int64; }
bool PrimitiveType::isBool() const                  { return type == Primitive::bool_; }
bool PrimitiveType::canBeVectorElementType() const  { return isInteger() || isFloatingPoint() || isFixed() || isBool() || isComplex(); }
bool PrimitiveType::canBeArrayElementType() const   { return isValid() && ! isVoid(); }

const char* PrimitiveType::getDescription() const
{
    switch (type)
    {
        case Primitive::void_:          return "void";
        case Primitive::float32:        return "float32";
        case Primitive::float64:        return "float64";
        case Primitive::fixed:          return "fixed";
        case Primitive::complex32:      return "complex32";
        case Primitive::complex64:      return "complex64";
        case Primitive::int32:          return "int32";
        case Primitive::int64:          return "int64";
        case Primitive::bool_:          return "bool";
        default:                        return "<unknown>";
    }
}

const char* PrimitiveType::getShortIdentifierDescription() const
{
    switch (type)
    {
        case Primitive::void_:          return "v";
        case Primitive::float32:        return "f32";
        case Primitive::float64:        return "f64";
        case Primitive::fixed:          return "fix";
        case Primitive::complex32:      return "c32";
        case Primitive::complex64:      return "c64";
        case Primitive::int32:          return "i32";
        case Primitive::int64:          return "i64";
        case Primitive::bool_:          return "b";
        default:                        return "_";
    }
}

uint64_t PrimitiveType::getPackedSizeInBytes() const
{
    switch (type)
    {
        case Primitive::void_:          return 1;
        case Primitive::float32:        return 4;
        case Primitive::float64:        return 8;
        case Primitive::fixed:          return 4;
        case Primitive::complex32:      return 8;
        case Primitive::complex64:      return 16;
        case Primitive::int32:          return 4;
        case Primitive::int64:          return 8;
        case Primitive::bool_:          return 1;
        default:                        SOUL_ASSERT_FALSE; return 1;
    }
}

//==============================================================================
Type::Type (Category t) : category (t) {}
Type::Type (Structure& s) : category (Category::structure), structure (s) {}

Type::Type (PrimitiveType t) : category (t.isValid() ? Category::primitive : Category::invalid), primitiveType (t) {}
Type::Type (PrimitiveType::Primitive t) : Type (PrimitiveType (t)) {}

Type Type::parse (std::string text)
{
    return heart::Parser::parsePrimitiveType (CodeLocation::createFromString ({}, std::move (text)));
}

//==============================================================================
bool Type::isEqual (const Type& other, int flags) const
{
    if (primitiveType != other.primitiveType)
        return false;

    if ((flags & ignoreReferences) == 0 && isRef != other.isRef)
        return false;

    if ((flags & ignoreConst) == 0 && isConstant != other.isConstant)
        return false;

    if (category != other.category)
    {
        if ((flags & ignoreVectorSize1) != 0)
            if ((isVectorOfSize1() && other.isPrimitive()) || (other.isVectorOfSize1() && isPrimitive()))
                return true;

        return false;
    }

    if (isSizedType())
    {
        if (boundingSize != other.boundingSize)
            return false;

        if (isArray())
            return getArrayElementType().isEqual (other.getArrayElementType(), flags);

        return true;
    }

    if (isStruct())
    {
        if (structure == other.structure)
            return true;

        if ((flags & duckTypeStructures) == 0)
            return false;

        auto& s1 = *structure;
        auto& s2 = *other.structure;

        if (s1.getNumMembers() != s2.getNumMembers())
            return false;

        for (size_t i = 0; i < s1.getNumMembers(); ++i)
            if (! s1.getMemberType (i).isEqual (s2.getMemberType (i), flags))
                return false;

        return true;
    }

    return true;
}

bool Type::isIdentical (const Type& other) const
{
    return isEqual (other, failOnAllDifferences);
}

bool Type::hasIdenticalLayout (const Type& other) const
{
    return isEqual (other, ignoreVectorSize1 | duckTypeStructures | ignoreConst);
}

bool Type::isPresentIn (choc::span<Type> types) const
{
    for (auto& t : types)
        if (isIdentical (t))
            return true;

    return false;
}

//==============================================================================
bool Type::isValid() const                { return category != Category::invalid; }
bool Type::isVoid() const                 { return primitiveType.isVoid(); }
bool Type::isFloat32() const              { return primitiveType.isFloat32(); }
bool Type::isFloat64() const              { return primitiveType.isFloat64(); }
bool Type::isFloatingPoint() const        { return primitiveType.isFloatingPoint(); }
bool Type::isComplex() const              { return primitiveType.isComplex(); }
bool Type::isFixed() const                { return primitiveType.isFixed(); }
bool Type::isComplex32() const            { return primitiveType.isComplex32(); }
bool Type::isComplex64() const            { return primitiveType.isComplex64(); }
bool Type::isInteger() const              { return primitiveType.isInteger(); }
bool Type::isInteger32() const            { return primitiveType.isInteger32(); }
bool Type::isInteger64() const            { return primitiveType.isInteger64(); }
bool Type::isBool() const                 { return primitiveType.isBool(); }
bool Type::isPrimitive() const            { return category == Category::primitive; }
bool Type::isPrimitiveOrVector() const    { return isPrimitive() || isVector(); }
bool Type::isPrimitiveInteger() const     { return isInteger() && isPrimitive(); }
bool Type::isPrimitiveFloat() const       { return isFloatingPoint() && isPrimitive(); }
bool Type::isPrimitiveComplex() const     { return isComplex() && isPrimitive(); }
bool Type::isPrimitiveBool() const        { return isBool() && isPrimitive(); }
bool Type::isVector() const               { return category == Category::vector; }
bool Type::isVectorOfSize1() const        { return isVector() && boundingSize == 1; }
bool Type::isArrayOrVector() const        { return isArray() || isVector(); }
bool Type::isArray() const                { return category == Category::array; }
bool Type::isUnsizedArray() const         { return isArray() && boundingSize == 0; }
bool Type::isFixedSizeArray() const       { return isArray() && boundingSize != 0; }
bool Type::isFixedSizeAggregate() const   { return isFixedSizeArray() || isVector() || isStruct(); }
bool Type::isStringLiteral() const        { return category == Category::stringLiteral; }
bool Type::isBoundedInt() const           { return isWrapped() || isClamped(); }
bool Type::isWrapped() const              { return category == Category::wrap; }
bool Type::isClamped() const              { return category == Category::clamp; }
bool Type::isSizedType() const            { return isArrayOrVector() || isBoundedInt(); }
bool Type::isScalar() const               { return isPrimitiveOrVector() && (isFloatingPoint() || isInteger()); }
bool Type::isStruct() const               { return category == Category::structure; }
bool Type::isReference() const            { return isRef; }
bool Type::isConst() const                { return isConstant; }
bool Type::isNonConstReference() const    { return isReference() && ! isConst(); }
bool Type::canBeVectorElementType() const { return isPrimitive() && primitiveType.canBeVectorElementType(); }
bool Type::canBeArrayElementType() const  { return isValid() && ! (isArray() || isReference() || isConst() || isVoid()); }


//==============================================================================
bool Type::isLegalVectorSize (int64_t size)
{
    return size > 0 && (uint64_t) size <= maxVectorSize;
}

Type Type::createVector (PrimitiveType element, ArraySize size)
{
    SOUL_ASSERT (element.canBeVectorElementType());
    SOUL_ASSERT (isLegalVectorSize ((int64_t) size));
    return createSizedType (element, Category::vector, size);
}

Type::ArraySize Type::getVectorSize() const
{
    if (isPrimitive())
        return 1;

    SOUL_ASSERT (isVector());
    return (ArraySize) boundingSize;
}

PrimitiveType Type::getVectorElementType() const
{
    SOUL_ASSERT (isVector());
    return primitiveType;
}

bool Type::canBeSafelyCastToArraySize (int64_t size)
{
    return size > 0 && (uint64_t) size <= maxArraySize;
}

Type Type::createArray (ArraySize size) const
{
    SOUL_ASSERT (canBeArrayElementType());

    Type t (Category::array);
    t.boundingSize = (BoundedIntSize) size;
    t.arrayElementCategory = category;
    t.primitiveType = primitiveType;

    if (isStruct())
        t.structure = getStructRef();
    else if (isSizedType())
        t.arrayElementBoundingSize = boundingSize;

    return t;
}

Type Type::createUnsizedArray() const
{
    return createArray (0);
}

void Type::resolveUnsizedArraySize (ArraySize newSize)
{
    SOUL_ASSERT (isUnsizedArray() && newSize > 0);
    boundingSize = (BoundedIntSize) newSize;
}

void Type::modifyArraySize (ArraySize newSize)
{
    SOUL_ASSERT (isArray());
    boundingSize = (BoundedIntSize) newSize;
}

Type::ArraySize Type::getArraySize() const      { SOUL_ASSERT (isArray()); return (ArraySize) boundingSize; }

Type Type::getArrayElementType() const
{
    SOUL_ASSERT (isArray());

    Type t (arrayElementCategory);
    t.primitiveType = primitiveType;
    t.boundingSize = arrayElementBoundingSize;
    t.structure = structure;
    return t;
}

Type::ArraySize Type::getArrayOrVectorSize() const
{
    SOUL_ASSERT (isArrayOrVector());
    return (ArraySize) boundingSize;
}

Type::ArraySize Type::getNumAggregateElements() const
{
    SOUL_ASSERT (isFixedSizeAggregate());
    return isStruct() ? getStructRef().getNumMembers() : getArrayOrVectorSize();
}

Type::ArraySize Type::getArrayElementVectorSize() const
{
    SOUL_ASSERT (isArray() && (arrayElementCategory == Category::primitive || arrayElementCategory == Category::vector));
    return (Type::ArraySize) arrayElementBoundingSize;
}

Type Type::createSizedType (PrimitiveType prim, Category type, ArraySize size)
{
    SOUL_ASSERT (prim.isValid());
    Type t (type);
    t.primitiveType = prim;
    t.boundingSize = (BoundedIntSize) size;
    return t;
}

Type Type::createCopyWithNewArraySize (ArraySize newSize) const
{
    SOUL_ASSERT (isArrayOrVector());

    Type t (*this);
    t.boundingSize = (BoundedIntSize) newSize;
    return t;
}

Type Type::createCopyWithNewArrayElementType (const Type& newElementType) const
{
    SOUL_ASSERT (isArray());
    auto t = newElementType.createArray (getArraySize());
    t.isConstant = isConstant;
    t.isRef = isRef;
    return t;
}

//==============================================================================
Type Type::createWrappedInt (BoundedIntSize size)
{
    SOUL_ASSERT (isLegalBoundedIntSize (size));
    return createSizedType (getBoundedIntSizeType(), Category::wrap, (ArraySize) size);
}

Type Type::createClampedInt (BoundedIntSize size)
{
    SOUL_ASSERT (isLegalBoundedIntSize (size));
    return createSizedType (getBoundedIntSizeType(), Category::clamp, (ArraySize) size);
}

Type Type::createWrappedInt (const Type& arrayOrVectorType)
{
    SOUL_ASSERT (arrayOrVectorType.isArrayOrVector());
    return createWrappedInt (arrayOrVectorType.isArray() ? (BoundedIntSize) arrayOrVectorType.getArraySize()
                                                         : (BoundedIntSize) arrayOrVectorType.getVectorSize());
}

Type Type::createClampedInt (const Type& arrayOrVectorType)
{
    SOUL_ASSERT (arrayOrVectorType.isArrayOrVector());
    return createClampedInt (arrayOrVectorType.isArray() ? (BoundedIntSize) arrayOrVectorType.getArraySize()
                                                         : (BoundedIntSize) arrayOrVectorType.getVectorSize());
}

Type::BoundedIntSize Type::getBoundedIntLimit() const   { SOUL_ASSERT (isBoundedInt()); return (BoundedIntSize) boundingSize; }
void Type::setBoundedIntLimit (BoundedIntSize newSize)  { SOUL_ASSERT (isBoundedInt()); boundingSize = newSize; }

bool Type::isBoundedIntWithinLimit (BoundedIntSize maxSize) const
{
    SOUL_ASSERT (isLegalBoundedIntSize (maxSize));
    return isBoundedInt() && getBoundedIntLimit() <= maxSize;
}

//==============================================================================
Type Type::createStruct (Structure& s)           { return Type (s); }
StructurePtr Type::getStruct() const             { SOUL_ASSERT (isStruct()); return structure; }
Structure& Type::getStructRef() const            { SOUL_ASSERT (isStruct()); return *structure; }

bool Type::usesStruct (const Structure& s) const
{
    if (structure != nullptr)
    {
        if (structure == s)
            return true;

        for (auto& m : structure->getMembers())
            if (m.type.usesStruct (s))
                return true;
    }

    return false;
}

Type Type::createStringLiteral()                 { return Type (Category::stringLiteral); }

//==============================================================================
Type Type::createReference() const           { SOUL_ASSERT (! isReference()); auto t = *this; t.isRef = true; return t; }
Type Type::removeReference() const           { SOUL_ASSERT (isReference()); return removeReferenceIfPresent(); }
Type Type::removeReferenceIfPresent() const  { auto t = *this; t.isRef = false; return t; }

Type Type::createConst() const               { SOUL_ASSERT (! isConst()); return createConstIfNotPresent(); }
Type Type::createConstIfNotPresent() const   { auto t = *this; t.isConstant = true; return t; }
Type Type::removeConst() const               { SOUL_ASSERT (isConst()); return removeConstIfPresent(); }
Type Type::removeConstIfPresent() const      { auto t = *this; t.isConstant = false; return t; }

Type Type::createConstReference() const      { return withConstAndRefFlags (true, true); }

Type Type::withConstAndRefFlags (bool shouldBeConst, bool shouldBeRef) const
{
    auto t = *this;
    t.isConstant = shouldBeConst;
    t.isRef = shouldBeRef;
    return t;
}

Type Type::getElementType() const
{
    if (isArray())      return getArrayElementType();
    if (isVector())     return getVectorElementType();
    if (isComplex32())  return PrimitiveType::float32;
    if (isComplex64())  return PrimitiveType::float64;

    SOUL_ASSERT_FALSE;
    return {};
}

PrimitiveType Type::getPrimitiveType() const
{
    SOUL_ASSERT (! (isArray() || isStruct()));
    return primitiveType;
}

std::string Type::getDescription (const std::function<std::string(const Structure&)>& getStructName) const
{
    if (isConst())          return "const " + removeConst().getDescription (getStructName);
    if (isReference())      return removeReference().getDescription (getStructName) + "&";
    if (isVector())         return primitiveType.getDescription() + ("<" + std::to_string (getVectorSize()) + ">");
    if (isUnsizedArray())   return getArrayElementType().getDescription (getStructName) + ("[]");
    if (isArray())          return getArrayElementType().getDescription (getStructName) + ("[" + std::to_string (getArraySize()) + "]");
    if (isWrapped())        return "wrap<" + std::to_string (getBoundedIntLimit()) + ">";
    if (isClamped())        return "clamp<" + std::to_string (getBoundedIntLimit()) + ">";
    if (isStruct())         return getStructName (getStructRef());
    if (isStringLiteral())  return "string";

    return primitiveType.getDescription();
}

std::string Type::getDescription() const
{
    return getDescription ([] (const Structure& s) { return s.getName(); });
}

std::string Type::getShortIdentifierDescription() const
{
    if (isConst())          return "const_" + removeConst().getShortIdentifierDescription();
    if (isReference())      return "ref_" + removeReference().getShortIdentifierDescription();
    if (isVector())         return "vec_" + std::to_string (getVectorSize()) + "_" + primitiveType.getShortIdentifierDescription();
    if (isUnsizedArray())   return "slice_" + getArrayElementType().getShortIdentifierDescription();
    if (isArray())          return "arr_" + std::to_string (getArraySize()) + "_" + getArrayElementType().getShortIdentifierDescription();
    if (isWrapped())        return "wrap_" + std::to_string (getBoundedIntLimit());
    if (isClamped())        return "clamp_" + std::to_string (getBoundedIntLimit());
    if (isStruct())         return "struct_" + getStructRef().getName();
    if (isStringLiteral())  return "string";

    return primitiveType.getShortIdentifierDescription();
}

uint64_t Type::getPackedSizeInBytes() const
{
    if (isVector())         return primitiveType.getPackedSizeInBytes() * static_cast<uint64_t> (getVectorSize());
    if (isUnsizedArray())   return sizeof (void*);
    if (isArray())          return getArrayElementType().getPackedSizeInBytes() * static_cast<uint64_t> (getArraySize());
    if (isStruct())         return structure->getPackedSizeInBytes();
    if (isStringLiteral())  return sizeof (StringDictionary::Handle);

    return primitiveType.getPackedSizeInBytes();
}

choc::value::Type Type::getExternalType() const
{
    if (isPrimitive())
    {
        if (isInteger32())  return choc::value::Type::createInt32();
        if (isInteger64())  return choc::value::Type::createInt64();
        if (isFloat32())    return choc::value::Type::createFloat32();
        if (isFloat64())    return choc::value::Type::createFloat64();
        if (isBool())       return choc::value::Type::createBool();
    }

    if (isVector())
    {
        auto size = static_cast<uint32_t> (getVectorSize());

        if (isInteger32())  return choc::value::Type::createVector<int32_t> (size);
        if (isInteger64())  return choc::value::Type::createVector<int64_t> (size);
        if (isFloat32())    return choc::value::Type::createVector<float>   (size);
        if (isFloat64())    return choc::value::Type::createVector<double>  (size);
        if (isBool())       return choc::value::Type::createVector<bool>    (size);
    }

    if (isArray())
        return choc::value::Type::createArray (getArrayElementType().getExternalType(),
                                               static_cast<uint32_t> (getArraySize()));

    if (isStruct())
    {
        auto& s = getStructRef();
        auto o = choc::value::Type::createObject (s.getName());

        for (auto& m : s.getMembers())
            o.addObjectMember (m.name, m.type.getExternalType());

        return o;
    }

    if (isStringLiteral())
        return choc::value::Type::createString();

    SOUL_ASSERT_FALSE;
    return {};
}

//==============================================================================
SubElementPath::SubElementPath() = default;
SubElementPath::SubElementPath (const SubElementPath&) = default;
SubElementPath::SubElementPath (SubElementPath&&) = default;
SubElementPath::~SubElementPath() = default;

SubElementPath::SubElementPath (size_t index)                     { indexes.push_back (index); }
SubElementPath& SubElementPath::operator+= (size_t index)         { indexes.push_back (index); return *this; }
SubElementPath SubElementPath::operator+ (size_t index) const     { auto p = *this; return p += index; }
choc::span<size_t> SubElementPath::getPath() const                { return indexes; }

SubElementPath::TypeAndOffset SubElementPath::getElement (const Type& parentType) const
{
    TypeAndOffset e { parentType, 0 };

    for (auto& index : indexes)
    {
        if (e.type.isArrayOrVector())
        {
            SOUL_ASSERT (! e.type.isUnsizedArray());
            SOUL_ASSERT (e.type.isValidArrayOrVectorIndex (index));

            e.type = e.type.getElementType();
            e.offset += ((size_t) e.type.getPackedSizeInBytes()) * index;
            continue;
        }

        if (e.type.isStruct())
        {
            auto& members = e.type.getStructRef().getMembers();
            SOUL_ASSERT (index < members.size());
            e.type = members[index].type;

            for (size_t i = 0; i < index; ++i)
                e.offset += (size_t) members[i].type.getPackedSizeInBytes();

            continue;
        }

        SOUL_ASSERT_FALSE;
    }

    return e;
}


} // namespace soul
