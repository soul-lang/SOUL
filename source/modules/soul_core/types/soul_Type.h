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

namespace soul
{

using StructurePtr = RefCountedPtr<Structure>;

//==============================================================================
/**
    Represents a SOUL type.
    @see PrimitiveType, TypeRules
*/
struct Type  final
{
    Type() = default;
    Type (PrimitiveType);
    Type (PrimitiveType::Primitive);

    Type (const Type&) = default;
    Type (Type&&) = default;
    Type& operator= (const Type&) = default;
    Type& operator= (Type&&) = default;

    //==============================================================================
    bool isValid() const;
    bool isVoid() const;
    bool isFloat32() const;
    bool isFloat64() const;
    bool isFloatingPoint() const;
    bool isFixed() const;
    bool isComplex() const;
    bool isComplex32() const;
    bool isComplex64() const;
    bool isInteger() const;
    bool isInteger32() const;
    bool isInteger64() const;
    bool isBool() const;
    bool isPrimitive() const;
    bool isPrimitiveOrVector() const;
    bool isPrimitiveFloat() const;
    bool isPrimitiveInteger() const;
    bool isPrimitiveComplex() const;
    bool isPrimitiveBool() const;
    bool isVector() const;
    bool isVectorOfSize1() const;
    bool isArrayOrVector() const;
    bool isArray() const;
    bool isUnsizedArray() const;
    bool isFixedSizeArray() const;
    bool isFixedSizeAggregate() const;
    bool isScalar() const;
    bool isStringLiteral() const;
    bool isBoundedInt() const;
    bool isWrapped() const;
    bool isClamped() const;
    bool isStruct() const;
    bool isReference() const;
    bool isConst() const;
    bool isNonConstReference() const;

    enum ComparisonFlags : int
    {
        failOnAllDifferences   = 0,
        ignoreReferences       = 1,
        ignoreConst            = 2,
        ignoreVectorSize1      = 4,
        duckTypeStructures     = 8
    };

    bool isEqual (const Type&, int comparisonFlags) const;
    bool isIdentical (const Type&) const;
    bool hasIdenticalLayout (const Type&) const;
    bool isPresentIn (choc::span<Type> types) const;

    //==============================================================================
    uint64_t getPackedSizeInBytes() const;
    bool isPackedSizeTooBig() const;

    //==============================================================================
    using ArraySize = size_t;

    static constexpr ArraySize maxVectorSize = 256;
    static bool isLegalVectorSize (int64_t size);
    static Type createVector (PrimitiveType elementType, ArraySize size);
    ArraySize getVectorSize() const;
    PrimitiveType getVectorElementType() const;
    bool canBeVectorElementType() const;
    bool canBeArrayElementType() const;

    //==============================================================================
    static constexpr uint64_t maxArraySize = std::numeric_limits<int32_t>::max();

    Type createArray (ArraySize size) const;
    Type createUnsizedArray() const;

    static bool canBeSafelyCastToArraySize (int64_t size);
    ArraySize getArraySize() const;
    Type getArrayElementType() const;

    void resolveUnsizedArraySize (ArraySize newSize);
    void modifyArraySize (ArraySize newSize);
    Type createCopyWithNewArraySize (ArraySize newSize) const;
    Type createCopyWithNewArrayElementType (const Type& newElementType) const;

    template <typename IntType>
    bool isValidArrayOrVectorIndex (IntType value) const
    {
        auto size = (int64_t) getArrayOrVectorSize();
        return (int64_t) value > -size && (int64_t) value < size;
    }

    template <typename IntType>
    ArraySize convertArrayOrVectorIndexToValidRange (IntType value) const
    {
        return value < 0 ? static_cast<ArraySize> ((int64_t) getArrayOrVectorSize() + (int64_t) value)
                         : static_cast<ArraySize> (value);
    }

    template <typename IntType>
    bool isValidArrayOrVectorRange (IntType start, IntType end) const
    {
        return isValidArrayOrVectorIndex (start)
                && (isValidArrayOrVectorIndex (end) || (ArraySize) end == getArrayOrVectorSize())
                && convertArrayOrVectorIndexToValidRange (start) < convertArrayOrVectorIndexToValidRange (end)
                && end != 0;
    }

    template <typename IntType>
    static ArraySize castToArraySize (IntType value)
    {
        SOUL_ASSERT (value >= 0 && value < (IntType) maxArraySize);
        return static_cast<ArraySize> (value);
    }

    using UnsizedArraySizeType = int32_t;
    static constexpr PrimitiveType getDynamicArraySizeType()    { return PrimitiveType::int32; }

    ArraySize getArrayOrVectorSize() const;
    ArraySize getNumAggregateElements() const;
    ArraySize getArrayElementVectorSize() const;

    //==============================================================================
    using BoundedIntSize = int32_t;
    static constexpr PrimitiveType getBoundedIntSizeType()      { return PrimitiveType::int32; }

    template <typename IntType>
    static constexpr bool isLegalBoundedIntSize  (IntType size)  { return size > 0 && (int64_t) size < (int64_t) std::numeric_limits<BoundedIntSize>::max(); }

    static Type createWrappedInt (BoundedIntSize size);
    static Type createClampedInt (BoundedIntSize size);
    static Type createWrappedInt (const Type& arrayOrVectorType);
    static Type createClampedInt (const Type& arrayOrVectorType);

    BoundedIntSize getBoundedIntLimit() const;
    void setBoundedIntLimit (BoundedIntSize newSize);
    bool isBoundedIntWithinLimit (BoundedIntSize maxSize) const;

    template <typename IntType>
    bool isValidBoundedIntIndex (IntType value) const       { SOUL_ASSERT (isBoundedInt()); return value >= 0 && (uint64_t) value < (uint64_t) boundingSize; }

    //==============================================================================
    static Type createStruct (Structure&);
    StructurePtr getStruct() const;
    Structure& getStructRef() const;
    bool usesStruct (const Structure&) const;

    //==============================================================================
    static Type createStringLiteral();

    //==============================================================================
    Type createReference() const;
    Type removeReference() const;
    Type removeReferenceIfPresent() const;

    Type createConst() const;
    Type createConstIfNotPresent() const;
    Type removeConst() const;
    Type removeConstIfPresent() const;

    Type createConstReference() const;
    Type withConstAndRefFlags (bool isConst, bool isRef) const;

    Type getElementType() const;
    PrimitiveType getPrimitiveType() const;

    static constexpr PrimitiveType getFrequencyType()    { return PrimitiveType::float64; }

    //==============================================================================
    /** Returns a SOUL syntax formatted description of this type. */
    std::string getDescription (const std::function<std::string(const Structure&)>& getStructNameFn) const;
    /** Returns a SOUL syntax formatted description of this type. */
    std::string getDescription() const;

    /** Returns a compact, identifier-friendly string equivalent to this type.
        This is handy for things like appending a type to a generated name. */
    std::string getShortIdentifierDescription() const;

    /** Creates a choc::value::Type which represents this type. */
    choc::value::Type getExternalType() const;

    /** Tries to parse a SOUL syntax string containing a type.
        This just returns an invalid Type if the parse fails - if you need error
        reporting then you'd need to use the parser classes directly.
    */
    static Type parse (std::string);

    //==============================================================================
private:
    enum class Category  : uint8_t
    {
        invalid,
        primitive,
        vector,
        array,
        wrap,
        clamp,
        structure,
        stringLiteral
    };

    Category category = Category::invalid, arrayElementCategory = Category::invalid;
    bool isRef = false;
    bool isConstant = false;
    PrimitiveType primitiveType;
    BoundedIntSize boundingSize = 0, arrayElementBoundingSize = 0;
    StructurePtr structure;

    explicit Type (Category);
    explicit Type (Structure&);
    bool isSizedType() const;
    static Type createSizedType (PrimitiveType, Category, ArraySize);

    bool operator== (const Type&) = delete; // the semantics of these are unclear: use isEqual() instead
    bool operator!= (const Type&) = delete;
};

//==============================================================================
/** A sequence of indexes which are used to drill-down from a top-level aggregate
    object to one of its (recursively) nested sub-elements.
*/
struct SubElementPath
{
    SubElementPath();
    SubElementPath (const SubElementPath&);
    SubElementPath (SubElementPath&&);
    ~SubElementPath();

    SubElementPath (size_t index);

    template <typename... Indexes>
    SubElementPath (size_t index, Indexes... otherIndexes)
    {
        indexes.push_back (index, otherIndexes...);
    }

    SubElementPath operator+ (size_t index) const;
    SubElementPath& operator+= (size_t index);

    struct TypeAndOffset
    {
        Type type;
        size_t offset;
    };

    TypeAndOffset getElement (const Type& parentType) const;

    choc::span<size_t> getPath() const;

private:
    ArrayWithPreallocation<size_t, 4> indexes;
};

} // namespace soul
