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
bool PrimitiveType::isInteger() const               { return type == Primitive::int32 || type == Primitive::int64; }
bool PrimitiveType::isInteger32() const             { return type == Primitive::int32; }
bool PrimitiveType::isInteger64() const             { return type == Primitive::int64; }
bool PrimitiveType::isBool() const                  { return type == Primitive::bool_; }
bool PrimitiveType::canBeVectorElementType() const  { return isInteger() || isFloatingPoint() || isFixed() || isBool(); }
bool PrimitiveType::canBeArrayElementType() const   { return isValid() && ! isVoid(); }

const char* PrimitiveType::getDescription() const
{
    switch (type)
    {
        case Primitive::void_:          return "void";
        case Primitive::float32:        return "float32";
        case Primitive::float64:        return "float64";
        case Primitive::fixed:          return "fixed";
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
        case Primitive::int32:          return "i32";
        case Primitive::int64:          return "i64";
        case Primitive::bool_:          return "b";
        default:                        return "_";
    }
}

size_t PrimitiveType::getPackedSizeInBytes() const
{
    switch (type)
    {
        case Primitive::void_:          return 1;
        case Primitive::float32:        return 4;
        case Primitive::float64:        return 8;
        case Primitive::fixed:          return 4;
        case Primitive::int32:          return 4;
        case Primitive::int64:          return 8;
        case Primitive::bool_:          return 1;
        default:                        SOUL_ASSERT_FALSE; return 1;
    }
}

//==============================================================================
Type::Type (Category t) : type (t) {}
Type::Type (Structure& s) : type (Category::structure), structure (s) {}

Type::Type (const Type& other)
    : type (other.type),
      primitiveType (other.primitiveType),
      boundingSize (other.boundingSize),
      structure (other.structure),
      isRef (other.isRef),
      isConstant (other.isConstant)
{
    if (auto* e = other.arrayElementType.get())
        arrayElementType.reset (new Type (*e));
}

Type& Type::operator= (const Type& other)
{
    type = other.type;
    primitiveType = other.primitiveType;
    boundingSize = other.boundingSize;
    structure = other.structure;
    isRef = other.isRef;
    isConstant = other.isConstant;

    if (auto* e = other.arrayElementType.get())
        arrayElementType.reset (new Type (*e));

    return *this;
}

Type::Type (PrimitiveType t) : type (t.isValid() ? Category::primitive : Category::invalid), primitiveType (t) {}
Type::Type (PrimitiveType::Primitive t) : Type (PrimitiveType (t)) {}

Type Type::parse (std::string text)
{
    return heart::Parser::parseType (CodeLocation::createFromString ({}, std::move (text)));
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

    if (type != other.type)
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

        if (s1.members.size() != s2.members.size())
            return false;

        for (size_t i = 0; i < s1.members.size(); ++i)
            if (! s1.members[i].type.isEqual (s2.members[i].type, flags))
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
    return isEqual (other, ignoreVectorSize1 | duckTypeStructures);
}

bool Type::isPresentIn (ArrayView<Type> types) const
{
    for (auto& t : types)
        if (isIdentical (t))
            return true;

    return false;
}

//==============================================================================
bool Type::isValid() const                { return type != Category::invalid; }
bool Type::isVoid() const                 { return primitiveType.isVoid(); }
bool Type::isFloat32() const              { return primitiveType.isFloat32(); }
bool Type::isFloat64() const              { return primitiveType.isFloat64(); }
bool Type::isFloatingPoint() const        { return primitiveType.isFloatingPoint(); }
bool Type::isInteger() const              { return primitiveType.isInteger(); }
bool Type::isInteger32() const            { return primitiveType.isInteger32(); }
bool Type::isInteger64() const            { return primitiveType.isInteger64(); }
bool Type::isBool() const                 { return primitiveType.isBool(); }
bool Type::isPrimitive() const            { return type == Category::primitive; }
bool Type::isPrimitiveOrVector() const    { return isPrimitive() || isVector(); }
bool Type::isPrimitiveInteger() const     { return isInteger() && isPrimitive(); }
bool Type::isPrimitiveFloat() const       { return isFloatingPoint() && isPrimitive(); }
bool Type::isPrimitiveBool() const        { return isBool() && isPrimitive(); }
bool Type::isVector() const               { return type == Category::vector; }
bool Type::isVectorOfSize1() const        { return isVector() && boundingSize == 1; }
bool Type::isArrayOrVector() const        { return isArray() || isVector(); }
bool Type::isArray() const                { return type == Category::array; }
bool Type::isUnsizedArray() const         { return isArray() && boundingSize == 0; }
bool Type::isFixedSizeArray() const       { return isArray() && boundingSize != 0; }
bool Type::isFixedSizeAggregate() const   { return isFixedSizeArray() || isVector() || isStruct(); }
bool Type::isStringLiteral() const        { return type == Category::stringLiteral; }
bool Type::isBoundedInt() const           { return isWrapped() || isClamped(); }
bool Type::isWrapped() const              { return type == Category::wrap; }
bool Type::isClamped() const              { return type == Category::clamp; }
bool Type::isSizedType() const            { return isArrayOrVector() || isBoundedInt(); }
bool Type::isScalar() const               { return isPrimitiveOrVector() && (isFloatingPoint() || isInteger()); }
bool Type::isStruct() const               { return type == Category::structure; }
bool Type::isReference() const            { return isRef; }
bool Type::isConst() const                { return isConstant; }
bool Type::isNonConstReference() const    { return isReference() && ! isConst(); }
bool Type::canBeVectorElementType() const { return isPrimitive() && primitiveType.canBeVectorElementType(); }
bool Type::canBeArrayElementType() const  { return isValid() && ! (isReference() || isConst() || isVoid()); }


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
    return size > 0 && (uint64_t) size < maxArraySize;
}

Type Type::createArray (ArraySize size) const
{
    SOUL_ASSERT (canBeArrayElementType());
    Type t (Category::array);
    t.boundingSize = (BoundedIntSize) size;
    t.arrayElementType.reset (new Type (*this));
    return t;
}

Type Type::createUnsizedArray() const
{
    return createArray (0);
}

void Type::resolveUnsizedArraySize (ArraySize newSize)
{
    SOUL_ASSERT (isUnsizedArray());
    SOUL_ASSERT (newSize > 0);
    boundingSize = (BoundedIntSize) newSize;
}

void Type::modifyArraySize (ArraySize newSize)
{
    SOUL_ASSERT (isArray());
    boundingSize = (BoundedIntSize) newSize;
}


Type::ArraySize Type::getArraySize() const      { SOUL_ASSERT (isArray()); return (ArraySize) boundingSize; }
const Type& Type::getArrayElementType() const   { SOUL_ASSERT (isArray()); return *arrayElementType; }

Type::ArraySize Type::getArrayOrVectorSize() const
{
    SOUL_ASSERT (isArrayOrVector());
    return (ArraySize) boundingSize;
}

Type::ArraySize Type::getNumAggregateElements() const
{
    SOUL_ASSERT (isFixedSizeAggregate());
    return isStruct() ? getStructRef().members.size() : getArrayOrVectorSize();
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

Type Type::createCopyWithNewArrayElementType (Type newType) const
{
    SOUL_ASSERT (isArray());

    Type t (*this);
    t.arrayElementType.reset (new Type (std::move (newType)));
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
bool Type::isStruct (const Structure& s) const   { return structure == s; }

Type Type::createStringLiteral()                 { return Type (Category::stringLiteral); }

//==============================================================================
Type Type::createReference() const           { SOUL_ASSERT (! isReference()); auto t = *this; t.isRef = true; return t; }
Type Type::removeReference() const           { SOUL_ASSERT (isReference()); return removeReferenceIfPresent(); }
Type Type::removeReferenceIfPresent() const  { auto t = *this; t.isRef = false; return t; }

Type Type::createConst() const               { SOUL_ASSERT (! isConst()); return createConstIfNotPresent(); }
Type Type::createConstIfNotPresent() const   { auto t = *this; t.isConstant = true; return t; }
Type Type::removeConst() const               { SOUL_ASSERT (isConst()); return removeConstIfPresent(); }
Type Type::removeConstIfPresent() const      { auto t = *this; t.isConstant = false; return t; }

Type Type::withConstAndRefFlags (bool shouldBeConst, bool shouldBeRef) const
{
    auto t = *this;
    t.isConstant = shouldBeConst;
    t.isRef = shouldBeRef;
    return t;
}

Type Type::getElementType() const
{
    if (isArray())    return getArrayElementType();
    if (isVector())   return getVectorElementType();

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
    if (isUnsizedArray())   return arrayElementType->getDescription (getStructName) + ("[]");
    if (isArray())          return arrayElementType->getDescription (getStructName) + ("[" + std::to_string (getArraySize()) + "]");
    if (isWrapped())        return "wrap<" + std::to_string (getBoundedIntLimit()) + ">";
    if (isClamped())        return "clamp<" + std::to_string (getBoundedIntLimit()) + ">";
    if (isStruct())         return getStructName (getStructRef());
    if (isStringLiteral())  return "string";

    return primitiveType.getDescription();
}

std::string Type::getDescription() const
{
    return getDescription ([] (const Structure& s) { return s.name; });
}

std::string Type::getShortIdentifierDescription() const
{
    if (isConst())          return "const_" + removeConst().getShortIdentifierDescription();
    if (isReference())      return "ref_" + removeReference().getShortIdentifierDescription();
    if (isVector())         return "vec_" + std::to_string (getVectorSize()) + "_" + primitiveType.getShortIdentifierDescription();
    if (isUnsizedArray())   return "slice_" + arrayElementType->getShortIdentifierDescription();
    if (isArray())          return "arr_" + std::to_string (getArraySize()) + "_" + arrayElementType->getShortIdentifierDescription();
    if (isWrapped())        return "wrap_" + std::to_string (getBoundedIntLimit());
    if (isClamped())        return "clamp_" + std::to_string (getBoundedIntLimit());
    if (isStruct())         return "struct_" + getStructRef().name;
    if (isStringLiteral())  return "string";

    return primitiveType.getShortIdentifierDescription();
}

size_t Type::getPackedSizeInBytes() const
{
    if (isVector())         return primitiveType.getPackedSizeInBytes() * static_cast<size_t> (getVectorSize());
    if (isUnsizedArray())   return sizeof (void*);
    if (isArray())          return arrayElementType->getPackedSizeInBytes() * static_cast<size_t> (getArraySize());
    if (isStruct())         return structure->getPackedSizeInBytes();
    if (isStringLiteral())  return sizeof (StringDictionary::Handle);

    return primitiveType.getPackedSizeInBytes();
}

bool Type::isPackedSizeTooBig() const
{
    return getPackedSizeInBytes() > maxPackedObjectSize;
}

//==============================================================================
SubElementPath::SubElementPath() = default;
SubElementPath::SubElementPath (const SubElementPath&) = default;
SubElementPath::SubElementPath (SubElementPath&&) = default;
SubElementPath::~SubElementPath() = default;

SubElementPath::SubElementPath (size_t index)                     { indexes.push_back (index); }
SubElementPath& SubElementPath::operator+= (size_t index)         { indexes.push_back (index); return *this; }
SubElementPath SubElementPath::operator+ (size_t index) const     { auto p = *this; return p += index; }
ArrayView<size_t> SubElementPath::getPath() const                 { return indexes; }

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
            e.offset += e.type.getPackedSizeInBytes() * index;
            continue;
        }

        if (e.type.isStruct())
        {
            auto& members = e.type.getStructRef().members;
            SOUL_ASSERT (index < members.size());
            e.type = members[index].type;

            for (size_t i = 0; i < index; ++i)
                e.offset += members[i].type.getPackedSizeInBytes();

            continue;
        }

        SOUL_ASSERT_FALSE;
    }

    return e;
}


} // namespace soul
