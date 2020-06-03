/*
    ██████ ██   ██  ██████   ██████
   ██      ██   ██ ██    ██ ██         Clean Header-Only Classes
   ██      ███████ ██    ██ ██         Copyright (C)2020 Julian Storer
   ██      ██   ██ ██    ██ ██
    ██████ ██   ██  ██████   ██████    https://github.com/julianstorer/choc

   This file is part of the CHOC C++ collection - see the github page to find out more.

   The code in this file is provided under the terms of the ISC license:

   Permission to use, copy, modify, and/or distribute this software for any purpose with
   or without fee is hereby granted, provided that the above copyright notice and this
   permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO
   THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT
   SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR
   ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
   CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
   OR PERFORMANCE OF THIS SOFTWARE.
*/

#ifndef CHOC_VALUE_POOL_HEADER_INCLUDED
#define CHOC_VALUE_POOL_HEADER_INCLUDED

#include <vector>
#include <string>
#include <cstring>
#include <algorithm>
#include <memory>

namespace choc::value
{

struct Value;
struct MemberNameAndType;
struct MemberNameAndValue;
struct ElementTypeAndOffset;

//==============================================================================
/** An exception object which is thrown by the Type, Value and ValueView classes when various
    runtime checks fail.
    @see Type, Value, ValueView
*/
struct Error { const char* description; };

/** Throws an error exception.
    Note that the message string is taken as a raw pointer and not copied, so must be a string literal.
    This is used by the Type, Value and ValueView classes.
    @see Type, Value, ValueView
*/
[[noreturn]] static void throwError (const char* errorMessage)     { throw Error { errorMessage }; }

/** Throws an Error with the given message if the condition argument is false.
    Note that the message string is taken as a raw pointer and not copied, so must be a string literal.
    This is used by the Type, Value and ValueView classes.
*/
static void check (bool condition, const char* errorMessage)       { if (! condition) throwError (errorMessage); }


//==============================================================================
/** A type class that can represent primitives, vectors, strings, arrays and objects.

    A Type can represent:
      - A primitive int32 or int64
      - A primitive float or double
      - A primitive bool
      - A vector of primitives
      - A string
      - An array of other Values
      - An object, which has a class name and a set of named members, each holding another Value.

    The Type class attempts to be small and allocation-free for simple types like primitives, vectors and
    arrays of vectors, but will use heap storage when given something more complex to represent.

    A Type can also be serialised and deserialised to a packed format.

    @see Value, ValueView
*/
struct Type  final
{
    Type() = default;
    Type (Type&& other);
    Type (const Type&);
    Type& operator= (Type&&);
    Type& operator= (const Type&);
    ~Type();

    bool isVoid() const                 { return mainType == MainType::void_; }
    bool isInt32() const                { return isType (MainType::int32); }
    bool isInt64() const                { return isType (MainType::int64); }
    bool isInt() const                  { return isType (MainType::int32, MainType::int64); }
    bool isFloat32() const              { return isType (MainType::float32); }
    bool isFloat64() const              { return isType (MainType::float64); }
    bool isFloat() const                { return isType (MainType::float32, MainType::float64); }
    bool isBool() const                 { return isType (MainType::boolean); }
    bool isPrimitive() const            { return isType (MainType::int32, MainType::int64, MainType::float32, MainType::float64, MainType::boolean); }
    bool isObject() const               { return isType (MainType::object); }
    bool isString() const               { return isType (MainType::string); }
    bool isVector() const               { return isType (MainType::vector); }
    bool isArray() const                { return isType (MainType::primitiveArray, MainType::complexArray); }
    bool isUniformArray() const;        /**< A uniform array is one where every element has the same type. */
    bool isArrayOfVectors() const;
    bool isVectorSize1() const;

    /** Returns the number of elements in an array, vector or object. Throws an Error if the type is void. */
    uint32_t getNumElements() const;

    /** If the type is an array or vector with a uniform element type, this returns it; if not, it throws an Error. */
    Type getElementType() const;

    /** Returns the type of a given element in this type if it's an array. If the type isn't an array or the index is
        out of bounds, it will throw an Error.
    */
    Type getArrayElementType (uint32_t index) const;

    /** Returns the name and type of one of the members if this type is an object; if not, or the index is out
        of range, then this will throw an Error exception.
    */
    const MemberNameAndType& getObjectMember (uint32_t index) const;

    /** If this is an object, this returns the index of the member with a given name. If the name isn't found, it
        will return -1, and if the type isn't an object, it will throw an Error exception.
    */
    int getObjectMemberIndex (std::string_view name) const;

    /** Returns the class-name of this type if it's an object, or throws an Error if it's not. */
    const std::string& getObjectClassName() const;

    bool operator== (const Type&) const;
    bool operator!= (const Type&) const;

    //==============================================================================
    static Type createInt32()           { return Type (MainType::int32); }
    static Type createInt64()           { return Type (MainType::int64); }
    static Type createFloat32()         { return Type (MainType::float32); }
    static Type createFloat64()         { return Type (MainType::float64); }
    static Type createBool()            { return Type (MainType::boolean); }
    static Type createString()          { return Type (MainType::string); }

    /** Creates a type based on the given template type. */
    template <typename PrimitiveType>
    static Type createPrimitive();

    //==============================================================================
    /** Creates a vector type based on the given template type and size. */
    template <typename PrimitiveType>
    static Type createVector (uint32_t numElements);

    static Type createVectorInt32   (uint32_t numElements)    { return Type (MainType::int32,   numElements); }
    static Type createVectorInt64   (uint32_t numElements)    { return Type (MainType::int64,   numElements); }
    static Type createVectorFloat32 (uint32_t numElements)    { return Type (MainType::float32, numElements); }
    static Type createVectorFloat64 (uint32_t numElements)    { return Type (MainType::float64, numElements); }
    static Type createVectorBool    (uint32_t numElements)    { return Type (MainType::boolean, numElements); }

    //==============================================================================
    /** Creates a type representing an empty array. Element types can be appended with addArrayElements(). */
    static Type createEmptyArray();

    /** Creates a type representing an array containing a set of elements of a fixed type. */
    static Type createArray (uint32_t numElements, Type elementType);

    /** Creates a type representing an array of primitives based on the templated type. */
    template <typename PrimitiveType>
    static Type createArray (uint32_t numArrayElements);

    /** Creates a type representing an array of vectors based on the templated type. */
    template <typename PrimitiveType>
    static Type createArrayOfVectors (uint32_t numArrayElements, uint32_t numVectorElements);

    /** Appends a group of array elements with the given to this type's definition.
        This will throw an Error if this isn't possible for various reasons.
    */
    void addArrayElements (Type elementType, uint32_t numElements);

    //==============================================================================
    /** Returns a type representing an empty object, with the given class name. */
    static Type createObject (std::string className);

    /** Appends a member to an object type, with the given name and type. This will throw an Error if
        this isn't possible for some reason.
    */
    void addObjectMember (std::string memberName, Type memberType);

    //==============================================================================
    /** Returns the size in bytes needed to store a value of this type. */
    size_t getValueDataSize() const;

    /** Returns true if this type, or any of its sub-types are a string. */
    bool usesStrings() const;

    /** Returns the type and packed-data position of one of this type's sub-elements. */
    ElementTypeAndOffset getElementTypeAndOffset (uint32_t index) const;

    //==============================================================================
    /** Stores a representation of this type in a packed data format.
        It can later be reloaded with deserialise().
        @see deserialise
    */
    std::vector<uint8_t> serialise() const;

    /*  Recreates a type from a serialised version that was created by the serialise() method.

        The data format is simple:
        Primitives:  type (1 byte)
        Vectors:     type (1 byte), num elements (packed int), primitive type (1 byte)
        Array:       type (1 byte), num groups (packed int), [num repetitions (packed int), element type (type)]*
        Object:      type (1 byte), num members (packed int), name (null-term string), [member type (type), member name (null-term string)]*

        Packed ints are stored as a sequence of bytes in little-endian order, where each byte contains
        7 bits of data + the top bit is set if another byte follows it.

        Any errors while reading the data will cause an Error exception to be thrown.
        @see serialise
    */
    static Type deserialise (const void* dataStart, const void* dataEnd);

private:
    //==============================================================================
    enum class MainType  : uint8_t
    {
        void_           = 0,
        int32           = 0x04,
        int64           = 0x08,
        float32         = 0x14,
        float64         = 0x18,
        boolean         = 0x01,
        string          = 0x24,
        vector          = 0x30,
        primitiveArray  = 0x40,
        complexArray    = 0x50,
        object          = 0x60
    };

    static constexpr uint32_t maxNumVectorElements = 256;
    static constexpr uint32_t maxNumArrayElements = 1024 * 1024;

    static constexpr uint32_t getPrimitiveSize (MainType t)   { return static_cast<uint32_t> (t) & 15; }

    friend struct ValueView;
    struct SerialisationHelpers;
    struct ComplexArray;
    struct Object;

    struct Vector
    {
        MainType elementType;
        uint32_t numElements;

        size_t getElementSize() const;
        size_t getValueDataSize() const;
        ElementTypeAndOffset getElementInfo (uint32_t) const;
        bool operator== (const Vector&) const;
    };

    struct PrimitiveArray
    {
        MainType elementType;
        uint32_t numElements, numVectorElements;

        Type getElementType() const;
        size_t getElementSize() const;
        size_t getValueDataSize() const;
        ElementTypeAndOffset getElementInfo (uint32_t) const;
        bool operator== (const PrimitiveArray&) const;
    };

    union Content
    {
        Object* object;
        ComplexArray* complexArray;
        Vector vector;
        PrimitiveArray primitiveArray;
    };

    MainType mainType = MainType::void_;
    Content content;

    template <typename... Types> bool isType (Types... types) const   { return ((mainType == types) || ...); }
    template <typename Type> static constexpr MainType selectMainType();

    explicit Type (MainType);
    explicit Type (MainType vectorElementType, uint32_t);
};

//==============================================================================
/** This holds the type and location of a sub-element of a Type.
    @see Type::getElementTypeAndOffset()
*/
struct ElementTypeAndOffset
{
    Type elementType;
    size_t offset;   ///< The byte position within its parent value of the data representing this element
};

//==============================================================================
/** A simple dictionary base-class for mapping strings onto integer handles.
    This is needed by the Value and ValueView classes.
    @see Value, ValueView
*/
class StringDictionary
{
public:
    StringDictionary() = default;
    virtual ~StringDictionary() = default;

    struct Handle
    {
        uint32_t handle = 0;

        bool operator== (Handle h) const        { return handle == h.handle; }
        bool operator!= (Handle h) const        { return handle != h.handle; }
        bool operator<  (Handle h) const        { return handle <  h.handle; }
    };

    virtual Handle getHandleForString (std::string_view text) = 0;
    virtual std::string_view getStringForHandle (Handle handle) const = 0;
};


//==============================================================================
/**
    Represents a view onto an object which can represent various types of primitive,
    array and object types.

    The ValueView and Value classes differ in that ValueView does not own the data that it
    points to, but Value does. A ValueView should be used as a temporary wrapper around some
    data whose lifetime can be trusted to outlive the ValueView object. As a rule-of-thumb, you
    should treat Value and Valueview in the same way as std::string and std::string_view, so
    a ValueView makes a great type for a function parameter, but probably shouldn't be used
    as a function return type unless you really know what you're doing.

    The purpose of these classes is to allow manipulation of complex, dynamically-typed objects
    where the data holding a value is stored in a contiguous, packed, well-specified data
    format, so that it can be manipulated directly as raw memory when necessary. The ValueView
    is a lightweight wrapper around a type and a pointer to the raw data containing a value of that
    type. The Value class provides the same interface, but also owns the storage needed, and can
    return a ValueView of itself.

    @see Type, Value, choc::json::toString()
*/
struct ValueView  final
{
    ValueView();                                             /**< Creates an empty value with a type of 'void'. */
    ValueView (Type&&, void* data, StringDictionary*);       /**< Creates a value using the given type and raw block of data. */
    ValueView (const Type&, void* data, StringDictionary*);  /**< Creates a value using the given type and raw block of data. */

    ValueView (const ValueView&) = default;
    ValueView& operator= (const ValueView&) = default;

    //==============================================================================
    bool isVoid() const                         { return type.isVoid(); }

    const Type& getType() const                 { return type; }

    bool isInt32() const                        { return type.isInt32(); }
    bool isInt64() const                        { return type.isInt64(); }
    bool isInt() const                          { return type.isInt(); }
    bool isFloat32() const                      { return type.isFloat32(); }
    bool isFloat64() const                      { return type.isFloat64(); }
    bool isFloat() const                        { return type.isFloat(); }
    bool isBool() const                         { return type.isBool(); }
    bool isPrimitive() const                    { return type.isPrimitive(); }
    bool isObject() const                       { return type.isObject(); }
    bool isString() const                       { return type.isString(); }
    bool isVector() const                       { return type.isVector(); }
    bool isArray() const                        { return type.isArray(); }

    //==============================================================================
    int32_t           getInt32() const;     /**< Retrieves the value if this is an int32, otherwise throws an Error exception. */
    int64_t           getInt64() const;     /**< Retrieves the value if this is an int64, otherwise throws an Error exception. */
    float             getFloat32() const;   /**< Retrieves the value if this is a float, otherwise throws an Error exception. */
    double            getFloat64() const;   /**< Retrieves the value if this is a double, otherwise throws an Error exception. */
    bool              getBool() const;      /**< Retrieves the value if this is a bool, otherwise throws an Error exception. */
    std::string_view  getString() const;    /**< Retrieves the value if this is a string, otherwise throws an Error exception. */

    explicit operator int32_t() const            { return getInt32(); }      /**< If the object is not an int32, this will throw an Error. */
    explicit operator int64_t() const            { return getInt64(); }      /**< If the object is not an int64, this will throw an Error. */
    explicit operator float() const              { return getFloat32(); }    /**< If the object is not a float, this will throw an Error. */
    explicit operator double() const             { return getFloat64(); }    /**< If the object is not a double, this will throw an Error. */
    explicit operator bool() const               { return getBool(); }       /**< If the object is not a bool, this will throw an Error. */
    explicit operator std::string_view() const   { return getString(); }     /**< If the object is not a string, this will throw an Error. */

    /** Attempts to cast this value to the given primitive target type. If the type is void or something that
        can't be cast, it will throw an exception. This will do some minor casting, such as ints to doubles,
        but won't attempt do any kind of string to number conversions.
    */
    template <typename TargetType> TargetType get() const;

    /** Attempts to get this value as the given target type, but if this isn't possible,
        returns the default value provided instead of throwing an Error.
    */
    template <typename TargetType> TargetType getWithDefault (TargetType defaultValue) const;

    /** Attempts to write a new value to the memory pointed to by this view, as long as the type
        provided exactly matches the value's type.
    */
    template <typename PrimitiveType> void set (PrimitiveType newValue);

    //==============================================================================
    /** If this object is a vector, array or object, this returns the number of items it contains; otherwise
        it will throw an Error exception.
    */
    uint32_t size() const;

    /** If this object is an array or vector, and the index is valid, this returns one of its elements.
        Throws an error exception if the object is not a vector or the index is out of range.
    */
    ValueView operator[] (int index) const;

    /** If this object is an array or vector, and the index is valid, this returns one of its elements.
        Throws an error exception if the object is not a vector or the index is out of range.
    */
    ValueView operator[] (uint32_t index) const;

    //==============================================================================
    struct Iterator;
    struct EndIterator {};

    /** Iterating a Value is only valid for an array, vector or object. */
    Iterator begin() const;
    EndIterator end() const     { return {}; }

    //==============================================================================
    /** Returns the class name of this object.
        This will throw an error if the value is not an object.
    */
    const std::string& getObjectClassName() const;

    /** Returns the name and value of a member by index.
        This will throw an error if the value is not an object of if the index is out of range. (Use
        size() to find out how many members there are). To get a named value from an object, you can
        use operator[].
        @see size
    */
    MemberNameAndValue getObjectMemberAt (uint32_t index) const;

    /** Returns the value of a named member, or a void value if no such member exists.
        This will throw an error if the value is not an object.
    */
    ValueView operator[] (std::string_view name) const;

    /** Returns the value of a named member, or a void value if no such member exists.
        This will throw an error if the value is not an object.
    */
    ValueView operator[] (const char* name) const;

    /** Returns true if this is an object and contains the given member name. */
    bool hasObjectMember (std::string_view name) const;

    template <typename Visitor>
    void visitObjectMembers (Visitor&&) const;

    //==============================================================================
    /** Creates a view directly onto a packed array of primitives.
        The ValueView that is returned will not take a copy of the data, so its lifetime must be managed by the caller.
    */
    template <typename ElementType>
    static ValueView createArray (ElementType* targetData, uint32_t numElements);

    /** Creates a view directly onto a packed array of vector primitives.
        The ValueView that is returned will not take a copy of the data, so its lifetime must be managed by the caller.
    */
    template <typename ElementType>
    static ValueView create2DArray (ElementType* targetData, uint32_t numArrayElements, uint32_t numVectorElements);

    ValueView withDictionary (StringDictionary* newDictionary)  { return ValueView (type, data, newDictionary); }

    void* getRawData()                   { return data; }
    const void* getRawData() const       { return data; }

private:
    //==============================================================================
    friend struct Value;
    Type type;
    uint8_t* data = nullptr;
    StringDictionary* stringDictionary = nullptr;

    ValueView (StringDictionary&);
    template <typename TargetType> TargetType readContentAs() const;
    template <typename TargetType> TargetType readPrimitive (Type::MainType) const;
    template <typename PrimitiveType> void setUnchecked (PrimitiveType);

    template <typename Type1> static constexpr bool matchesType()                                       { return false; }
    template <typename Type1, typename Type2, typename... Type3> static constexpr bool matchesType()    { return std::is_same<const Type1, const Type2>::value || matchesType<Type1, Type3...>(); }
    template <typename PrimitiveType> static constexpr bool isValidPrimitiveType()                      { return matchesType<PrimitiveType, int32_t, int64_t, float, double, bool, StringDictionary::Handle>(); }

    ValueView operator[] (const void*) const = delete;
    ValueView operator[] (bool) const = delete;
};


//==============================================================================
/** Represents the name and type of a member in an object.
    @see Type
*/
struct MemberNameAndType
{
    std::string name;
    Type type;
};

/** Represents the name and value of a member in an object.
    @see Value, ValueView
*/
struct MemberNameAndValue
{
    const char* name;
    ValueView value;
};


//==============================================================================
/**
    Stores a value of any type that the Type class can represent.

    A Value class can be treated as a by-value class, and manages all the storage needed to
    represent a ValueView object.

    The ValueView and Value classes differ in that ValueView does not own the data that it
    points to, but Value does. A ValueView should be used as a temporary wrapper around some
    data whose lifetime can be trusted to outlive the ValueView object.

    The purpose of these classes is to allow manipulation of complex, dynamically-typed objects
    where the data holding a value is stored in a contiguous, packed, well-specified data
    format, so that it can be manipulated directly as raw memory when necessary. The ValueView
    is a lightweight wrapper around a type and a pointer to the raw data containing a value of that
    type. The Value class provides the same interface, but also owns the storage needed, and can
    return a ValueView of itself.

    The Value class is versatile enough, and close enough to JSON's architecture that it can be
    parsed and printed as JSON (though storing a Value as JSON will be a slightly lossy operation
    as JSON has fewer types).

    @see ValueView, Type, choc::json::parse(), choc::json::toString()
*/
struct Value   final
{
    /** Creates an empty value with a type of 'void'. */
    Value();

    Value (Value&&);
    Value (const Value&);
    Value& operator= (Value&&);
    Value& operator= (const Value&);

    /** Creates a zero-initialised value with the given type. */
    explicit Value (const Type&);

    /** Creates a zero-initialised value with the given type. */
    explicit Value (Type&&);

    /** Creates a deep-copy of the given ValueView. */
    explicit Value (const ValueView&);

    /** Creates a deep-copy of the given ValueView. */
    Value& operator= (const ValueView&);

    static Value createInt32   (int32_t);
    static Value createInt64   (int64_t);
    static Value createFloat32 (float);
    static Value createFloat64 (double);
    static Value createBool    (bool);

    static Value createPrimitive (int32_t);
    static Value createPrimitive (int64_t);
    static Value createPrimitive (float);
    static Value createPrimitive (double);
    static Value createPrimitive (bool);

    static Value createString (std::string_view);

    //==============================================================================
    /** Allocates a vector, populating it from an array of primitive values. */
    template <typename ElementType>
    static Value createVector (const ElementType* sourceElements, uint32_t numElements);

    /** Allocates a vector, populating it using a functor to return the initial primitive values.
        The functor must be a class or lambda which takes a uint32_t index parameter and returns
        the primitive value for that index. The type of the returned primitive is used as the
        vector's element type.
    */
    template <typename GetElementValue>
    static Value createVector (uint32_t numElements, const GetElementValue& getValueForIndex);

    //==============================================================================
    /** Creates an empty array (to which elements can then be appended with addArrayElement) */
    static Value createEmptyArray();

    /** Allocates an array, populating it using a functor to return the initial values.
        The functor must be a class or lambda which takes a uint32_t index parameter and returns
        either Value objects or primitive types to store at that index.
    */
    template <typename GetElementValue>
    static Value createArray (uint32_t numElements, const GetElementValue& getValueForIndex);

    /** Allocates an array which is a packed array of vector primitives, populating it using a
        functor to return the initial values.
        The functor must be a class or lambda which takes two uint32_t index parameters (the outer
        and inner indices for the required element) and returns a primitive type to store at that
        location.
    */
    template <typename GetElementValue>
    static Value createArray (uint32_t numArrayElements, uint32_t numVectorElements, const GetElementValue& getValueAt);

    /** Allocates a copy of a packed array of vector primitives. */
    template <typename ElementType>
    static Value create2DArray (const ElementType* sourceElements, uint32_t numArrayElements, uint32_t numVectorElements);

    /** Appends an element to this object, if it's an array. If not, then this will throw an Error exception. */
    void addArrayElement (const ValueView&);

    //==============================================================================
    /** Creates a new empty object. */
    static Value createObject (std::string className);

    /** Appends a member to an object, with the given name and value. This will throw an Error if
        this isn't possible for some reason.
    */
    void addObjectMember (std::string memberName, const ValueView& memberValue);

    //==============================================================================
    bool isVoid() const                         { return value.isVoid(); }
    bool isInt32() const                        { return value.isInt32(); }
    bool isInt64() const                        { return value.isInt64(); }
    bool isInt() const                          { return value.isInt(); }
    bool isFloat32() const                      { return value.isFloat32(); }
    bool isFloat64() const                      { return value.isFloat64(); }
    bool isFloat() const                        { return value.isFloat(); }
    bool isBool() const                         { return value.isBool(); }
    bool isPrimitive() const                    { return value.isPrimitive(); }
    bool isObject() const                       { return value.isObject(); }
    bool isString() const                       { return value.isString(); }
    bool isVector() const                       { return value.isVector(); }
    bool isArray() const                        { return value.isArray(); }

    //==============================================================================
    int32_t           getInt32() const          { return value.getInt32(); }      /**< Retrieves the value if this is an int32, otherwise throws an Error exception. */
    int64_t           getInt64() const          { return value.getInt64(); }      /**< Retrieves the value if this is an int64, otherwise throws an Error exception. */
    float             getFloat32() const        { return value.getFloat32(); }    /**< Retrieves the value if this is a float, otherwise throws an Error exception. */
    double            getFloat64() const        { return value.getFloat64(); }    /**< Retrieves the value if this is a double, otherwise throws an Error exception. */
    bool              getBool() const           { return value.getBool(); }       /**< Retrieves the value if this is a bool, otherwise throws an Error exception. */
    std::string_view  getString() const         { return value.getString(); }     /**< Retrieves the value if this is a string, otherwise throws an Error exception. */

    explicit operator int32_t() const           { return value.getInt32(); }      /**< If the object is not an int32, this will throw an Error. */
    explicit operator int64_t() const           { return value.getInt64(); }      /**< If the object is not an int64, this will throw an Error. */
    explicit operator float() const             { return value.getFloat32(); }    /**< If the object is not a float, this will throw an Error. */
    explicit operator double() const            { return value.getFloat64(); }    /**< If the object is not a double, this will throw an Error. */
    explicit operator bool() const              { return value.getBool(); }       /**< If the object is not a bool, this will throw an Error. */
    explicit operator std::string_view() const  { return value.getString(); }     /**< If the object is not a string, this will throw an Error. */

    /** Attempts to cast this value to the given primitive target type. If the type is void or something that
        can't be cast, it will throw an exception. This will do some minor casting, such as ints to doubles,
        but won't attempt do any kind of string to number conversions.
    */
    template <typename TargetType> TargetType get() const;

    /** Attempts to get this value as the given target type, but if this isn't possible,
        returns the default value provided instead of throwing an Error.
    */
    template <typename TargetType> TargetType getWithDefault (TargetType defaultValue) const;

    /** If this object is a vector, array or object, this returns the number of items it contains; otherwise
        it will throw an Error exception.
    */
    uint32_t size() const                                               { return value.size(); }

    /** If this object is an array or vector, and the index is valid, this returns one of its elements.
        Note that this returns a view of the parent Value, which will become invalid as soon as any
        change is made to the parent Value.
        Throws an error exception if the object is not a vector or the index is out of range.
    */
    ValueView operator[] (int index) const                              { return value[index]; }

    /** If this object is an array or vector, and the index is valid, this returns one of its elements.
        Note that this returns a view of the parent Value, which will become invalid as soon as any
        change is made to the parent Value.
        Throws an error exception if the object is not a vector or the index is out of range.
    */
    ValueView operator[] (uint32_t index) const                         { return value[index]; }

    //==============================================================================
    /** Iterating a Value is only valid for an array, vector or object. */
    ValueView::Iterator begin() const;
    ValueView::EndIterator end() const;

    //==============================================================================
    /** Returns the class name of this object.
        This will throw an error if the value is not an object.
    */
    const std::string& getObjectClassName() const                       { return value.getObjectClassName(); }

    /** Returns the name and value of a member by index.
        This will throw an error if the value is not an object of if the index is out of range. (Use
        size() to find out how many members there are). To get a named value from an object, you can
        use operator[].
        @see size
    */
    MemberNameAndValue getObjectMemberAt (uint32_t index) const         { return value.getObjectMemberAt (index); }

    /** Returns the value of a named member, or a void value if no such member exists.
        Note that this returns a view of the parent Value, which will become invalid as soon as any
        change is made to the parent Value.
        This will throw an error if the value is not an object.
    */
    ValueView operator[] (std::string_view name) const                  { return value[name]; }

    /** Returns the value of a named member, or a void value if no such member exists.
        Note that this returns a view of the parent Value, which will become invalid as soon as any
        change is made to the parent Value.
        This will throw an error if the value is not an object.
    */
    ValueView operator[] (const char* name) const                       { return value[name]; }

    /** Returns true if this is an object and contains the given member name. */
    bool hasObjectMember (std::string_view name) const                  { return value.hasObjectMember (name); }

    /** Returns a ValueView of this Value. The ValueView will become invalid as soon as any change is made to this Value. */
    operator const ValueView&() const                                   { return value; }

    /** Returns a ValueView of this Value. The ValueView will become invalid as soon as any change is made to this Value. */
    const ValueView& getView() const                                    { return value; }

    /** Returns a mutable reference to the ValueView held inside this Value. This is only for use if you know what you're doing. */
    ValueView& getViewReference()                                       { return value; }

    /** Returns the type of this value. */
    const Type& getType() const                                         { return value.getType(); }

    /** Returns a pointer to the raw data that stores this value. */
    const void* getRawData() const                                      { return packedData.data(); }
    /** Returns a pointer to the raw data that stores this value. */
    void* getRawData()                                                  { return packedData.data(); }
    /** Returns the size of the raw data that stores this value. */
    size_t getRawDataSize() const                                       { return packedData.size(); }

private:
    //==============================================================================
    void appendValue (ValueView);
    void importStringHandles (ValueView&, const StringDictionary& old);

    struct SimpleStringDictionary  : public StringDictionary
    {
        Handle getHandleForString (std::string_view text) override;
        std::string_view getStringForHandle (Handle handle) const override;
        std::vector<std::string> strings;
    };

    std::vector<uint8_t> packedData;
    SimpleStringDictionary dictionary;
    ValueView value;
};


//==============================================================================
//        _        _           _  _
//     __| |  ___ | |_   __ _ (_)| | ___
//    / _` | / _ \| __| / _` || || |/ __|
//   | (_| ||  __/| |_ | (_| || || |\__ \ _  _  _
//    \__,_| \___| \__| \__,_||_||_||___/(_)(_)(_)
//
//   Code beyond this point is implementation detail...
//
//==============================================================================

inline size_t Type::Vector::getElementSize() const    { return getPrimitiveSize (elementType); }
inline size_t Type::Vector::getValueDataSize() const  { return getElementSize() * numElements; }

inline ElementTypeAndOffset Type::Vector::getElementInfo (uint32_t index) const
{
    check (index < numElements, "Index out of range");
    auto size = getElementSize();
    return { Type (elementType), size * index };
}

inline bool Type::Vector::operator== (const Vector& other) const  { return elementType == other.elementType && numElements == other.numElements; }

inline size_t Type::PrimitiveArray::getElementSize() const   { auto sz = getPrimitiveSize (elementType); if (numVectorElements != 0) sz *= numVectorElements; return sz; }
inline size_t Type::PrimitiveArray::getValueDataSize() const { return getElementSize() * numElements; }
inline Type Type::PrimitiveArray::getElementType() const     { return numVectorElements != 0 ? Type (elementType, numVectorElements) : Type (elementType); }

inline ElementTypeAndOffset Type::PrimitiveArray::getElementInfo (uint32_t index) const
{
    check (index < numElements, "Index out of range");
    auto primitiveSize = getPrimitiveSize (elementType);

    if (numVectorElements != 0)
        return { Type (elementType, numVectorElements), primitiveSize * numVectorElements * index };

    return { Type (elementType), primitiveSize * index };
}

inline bool Type::PrimitiveArray::operator== (const PrimitiveArray& other) const
{
    return elementType == other.elementType && numElements == other.numElements && numVectorElements == other.numVectorElements;
}

struct Type::ComplexArray
{
    uint32_t size() const
    {
        uint32_t total = 0;

        for (auto& g : groups)
            total += g.repetitions;

        return total;
    }

    Type getElementType (uint32_t index) const
    {
        uint32_t count = 0;

        for (auto& g : groups)
        {
            count += g.repetitions;

            if (index < count)
                return g.elementType;
        }

        throwError ("Index out of range");
    }

    size_t getValueDataSize() const
    {
        size_t total = 0;

        for (auto& g : groups)
            total += g.repetitions * g.elementType.getValueDataSize();

        return total;
    }

    bool usesStrings() const
    {
        for (auto& g : groups)
            if (g.elementType.usesStrings())
                return true;

        return false;
    }

    ElementTypeAndOffset getElementInfo (uint32_t index) const
    {
        size_t offset = 0;

        for (auto& g : groups)
        {
            auto elementSize = g.elementType.getValueDataSize();

            if (index < g.repetitions)
                return { g.elementType, offset + elementSize * index };

            index -= g.repetitions;
            offset += elementSize * g.repetitions;
        }

        throwError ("Index out of range");
    }

    void addElements (Type&& elementType, uint32_t numElementsToAdd)
    {
        if (! groups.empty() && groups.back().elementType == elementType)
            groups.back().repetitions += numElementsToAdd;
        else
            groups.push_back ({ numElementsToAdd, std::move (elementType) });
    }

    bool operator== (const ComplexArray& other) const   { return groups == other.groups; }
    bool isArrayOfVectors() const                       { return groups.size() == 1 && groups.front().elementType.isVector(); }
    bool isUniform() const                              { return groups.empty() || groups.size() == 1; }

    Type getUniformType() const
    {
        check (groups.size() == 1, "This array does not contain a single element type");
        return groups.front().elementType;
    }

    struct RepeatedGroup
    {
        uint32_t repetitions;
        Type elementType;

        bool operator== (const RepeatedGroup& other) const   { return repetitions == other.repetitions
                                                                   && elementType == other.elementType; }
    };

    std::vector<RepeatedGroup> groups;
};

struct Type::Object
{
    std::string className;
    std::vector<MemberNameAndType> members;

    size_t getValueDataSize() const
    {
        size_t total = 0;

        for (auto& m : members)
            total += m.type.getValueDataSize();

        return total;
    }

    bool usesStrings() const
    {
        for (auto& m : members)
            if (m.type.usesStrings())
                return true;

        return false;
    }

    ElementTypeAndOffset getElementInfo (uint32_t index) const
    {
        size_t offset = 0;

        for (size_t i = 0; i < members.size(); ++i)
        {
            if (i == index)
                return { members[i].type, offset };

            offset += members[i].type.getValueDataSize();
        }

        throwError ("Index out of range");
    }

    bool operator== (const Object& other) const
    {
        if (className != other.className)
            return false;

        if (members.size() != other.members.size())
            return false;

        for (size_t i = 0; i < members.size(); ++i)
            if (members[i].name != other.members[i].name
                    || members[i].type != other.members[i].type)
                return false;

        return true;
    }
};

inline Type::Type (Type&& other) : mainType (other.mainType), content (other.content)
{
    other.mainType = MainType::void_;
}

inline Type::Type (const Type& other) : mainType (other.mainType)
{
    if (isType (MainType::complexArray))   content.complexArray = new ComplexArray (*other.content.complexArray);
    else if (isObject())                   content.object = new Object (*other.content.object);
    else                                   content = other.content;
}

inline Type& Type::operator= (Type&& other)
{
    mainType = other.mainType;
    content = other.content;
    other.mainType = MainType::void_;
    return *this;
}

inline Type& Type::operator= (const Type& other)
{
    if (isType (MainType::complexArray))   delete content.complexArray;
    else if (isObject())                   delete content.object;

    mainType = other.mainType;

    if (isType (MainType::complexArray))   content.complexArray = new ComplexArray (*other.content.complexArray);
    else if (isObject())                   content.object = new Object (*other.content.object);
    else                                   content = other.content;

    return *this;
}

inline Type::Type (MainType t)  : mainType (t) {}

inline Type::Type (MainType vectorElementType, uint32_t size)  : mainType (MainType::vector)
{
    check (size <= maxNumVectorElements, "Too many vector elements");
    content.vector = { vectorElementType, size };
}

inline Type::~Type()
{
    if (isType (MainType::complexArray))   delete content.complexArray;
    else if (isObject())                   delete content.object;
}

inline bool Type::isUniformArray() const     { return isType (MainType::primitiveArray) || (isType (MainType::complexArray) && content.complexArray->isUniform()); }
inline bool Type::isArrayOfVectors() const   { return isType (MainType::primitiveArray); }
inline bool Type::isVectorSize1() const      { return isVector() && content.vector.numElements == 1; }

inline uint32_t Type::getNumElements() const
{
    if (isVector())                         return content.vector.numElements;
    if (isType (MainType::primitiveArray))  return content.primitiveArray.numElements;
    if (isType (MainType::complexArray))    return content.complexArray->size();
    if (isObject())                         return static_cast<uint32_t> (content.object->members.size());
    if (isPrimitive() || isString())        return 1;

    throwError ("This type doesn't have sub-elements");
}

inline Type Type::getElementType() const
{
    if (isVector())                         return Type (content.vector.elementType);
    if (isType (MainType::primitiveArray))  return content.primitiveArray.getElementType();
    if (isType (MainType::complexArray))    return content.complexArray->getUniformType();

    throwError ("This type is not an array or vector");
}

inline Type Type::getArrayElementType (uint32_t index) const
{
    if (isType (MainType::primitiveArray))  return content.primitiveArray.getElementType();
    if (isType (MainType::complexArray))    return content.complexArray->getElementType (index);
    throwError ("This type is not an array");
}

inline const MemberNameAndType& Type::getObjectMember (uint32_t index) const
{
    check (isObject(), "This type is not an object");
    check (index < content.object->members.size(), "Index out of range");
    return content.object->members[index];
}

inline int Type::getObjectMemberIndex (std::string_view name) const
{
    check (isObject(), "This type is not an object");
    int i = 0;

    for (auto& m : content.object->members)
    {
        if (m.name == name)
            return i;

        ++i;
    }

    return -1;
}

template <typename PrimitiveType>
inline constexpr Type::MainType Type::selectMainType()
{
    if constexpr (std::is_same<const PrimitiveType, const int32_t>::value)       return MainType::int32;
    if constexpr (std::is_same<const PrimitiveType, const int64_t>::value)       return MainType::int64;
    if constexpr (std::is_same<const PrimitiveType, const float>::value)         return MainType::float32;
    if constexpr (std::is_same<const PrimitiveType, const double>::value)        return MainType::float64;
    if constexpr (std::is_same<const PrimitiveType, const bool>::value)          return MainType::boolean;
    if constexpr (std::is_same<const PrimitiveType, const char* const>::value)   return MainType::string;
    if constexpr (std::is_same<const PrimitiveType, const std::string>::value)   return MainType::string;

    return MainType::void_;
}

template <typename PrimitiveType>
Type Type::createPrimitive()
{
    constexpr auto type = selectMainType<PrimitiveType>();
    static_assert (type != MainType::void_, "The template type needs to be one of the supported primitive types");
    return Type (type);
}

template <typename PrimitiveType>
Type Type::createVector (uint32_t numElements)
{
    constexpr auto type = selectMainType<PrimitiveType>();
    static_assert (type != MainType::void_, "The template type needs to be one of the supported primitive types");
    return Type (type, numElements);
}

inline Type Type::createEmptyArray()
{
    Type t (MainType::primitiveArray);
    t.content.primitiveArray.elementType = MainType::void_;
    t.content.primitiveArray.numElements = 0;
    t.content.primitiveArray.numVectorElements = 0;
    return t;
}

inline Type Type::createArray (uint32_t numElements, Type elementType)
{
    check (! elementType.isVoid(), "Type is void");
    check (numElements < maxNumArrayElements, "Too many array elements");

    if (elementType.isPrimitive())
    {
        Type t (MainType::primitiveArray);
        t.content.primitiveArray.elementType = elementType.mainType;
        t.content.primitiveArray.numElements = numElements;
        t.content.primitiveArray.numVectorElements = 0;
        return t;
    }

    if (elementType.isVector())
    {
        Type t (MainType::primitiveArray);
        t.content.primitiveArray.elementType = elementType.content.vector.elementType;
        t.content.primitiveArray.numElements = numElements;
        t.content.primitiveArray.numVectorElements = elementType.content.vector.numElements;
        return t;
    }

    Type t (MainType::complexArray);
    t.content.complexArray = new ComplexArray();
    t.content.complexArray->groups.push_back ({ numElements, std::move (elementType) });
    return t;
}

template <typename PrimitiveType>
Type Type::createArray (uint32_t numArrayElements)
{
    return createArrayOfVectors<PrimitiveType> (numArrayElements, 0);
}

template <typename PrimitiveType>
Type Type::createArrayOfVectors (uint32_t numArrayElements, uint32_t numVectorElements)
{
    constexpr auto elementType = selectMainType<PrimitiveType>();
    static_assert (elementType != MainType::void_, "The element type needs to be one of the supported primitive types");
    Type t (MainType::primitiveArray);
    t.content.primitiveArray.elementType = elementType;
    t.content.primitiveArray.numElements = numArrayElements;
    t.content.primitiveArray.numVectorElements = numVectorElements;
    return t;
}

inline void Type::addArrayElements (Type elementType, uint32_t numElementsToAdd)
{
    check (! elementType.isVoid(), "Element type cannot be void");

    if (isType (MainType::primitiveArray))
    {
        if (elementType == content.primitiveArray.getElementType())
        {
            content.primitiveArray.numElements += numElementsToAdd;
            return;
        }

        if (content.primitiveArray.numElements == 0)
        {
            *this = createArray (numElementsToAdd, elementType);
            return;
        }

        mainType = MainType::complexArray;
        auto newArray = new ComplexArray();
        newArray->groups.push_back ({ content.primitiveArray.numElements, content.primitiveArray.getElementType() });
        content.complexArray = newArray;
    }
    else
    {
        check (isType (MainType::complexArray), "Cannot add new elements to this type");
    }

    content.complexArray->addElements (std::move (elementType), numElementsToAdd);
}

inline Type Type::createObject (std::string className)
{
    Type t (MainType::object);
    t.content.object = new Object();
    t.content.object->className = std::move (className);
    return t;
}

inline void Type::addObjectMember (std::string memberName, Type memberType)
{
    check (! memberType.isVoid(), "Member type cannot be void");
    check (getObjectMemberIndex (memberName) < 0, "This object already contains a member with the given name");
    content.object->members.push_back ({ std::move (memberName), std::move (memberType) });
}

inline const std::string& Type::getObjectClassName() const
{
    check (isObject(), "This type is not an object");
    return content.object->className;
}

inline bool Type::operator== (const Type& other) const
{
    if (mainType != other.mainType)
        return false;

    if (isVector())                         return content.vector == other.content.vector;
    if (isType (MainType::primitiveArray))  return content.primitiveArray == other.content.primitiveArray;
    if (isType (MainType::complexArray))    return *content.complexArray == *other.content.complexArray;
    if (isObject())                         return *content.object == *other.content.object;

    return true;
}

inline bool Type::operator!= (const Type& other) const  { return ! operator== (other); }

inline size_t Type::getValueDataSize() const
{
    switch (mainType)
    {
        case MainType::int32:
        case MainType::float32:         return 4;
        case MainType::int64:
        case MainType::float64:         return 8;
        case MainType::boolean:         return 1;
        case MainType::string:          return sizeof (StringDictionary::Handle::handle);
        case MainType::vector:          return content.vector.getValueDataSize();
        case MainType::primitiveArray:  return content.primitiveArray.getValueDataSize();
        case MainType::complexArray:    return content.complexArray->getValueDataSize();
        case MainType::object:          return content.object->getValueDataSize();
        case MainType::void_:
        default:                        throwError ("Invalid type");
    }
}

inline bool Type::usesStrings() const
{
    return isString()
            || (isObject() && content.object->usesStrings())
            || (isType (MainType::complexArray) && content.complexArray->usesStrings());
}

inline ElementTypeAndOffset Type::getElementTypeAndOffset (uint32_t index) const
{
    if (isType (MainType::vector))          return content.vector.getElementInfo (index);
    if (isType (MainType::primitiveArray))  return content.primitiveArray.getElementInfo (index);
    if (isType (MainType::complexArray))    return content.complexArray->getElementInfo (index);
    if (isType (MainType::object))          return content.object->getElementInfo (index);

    throwError ("Invalid type");
}

//==============================================================================
struct Type::SerialisationHelpers
{
    enum class EncodedType  : uint8_t
    {
        void_       = 0,
        int32       = 1,
        int64       = 2,
        float32     = 3,
        float64     = 4,
        boolean     = 5,
        vector      = 6,
        array       = 7,
        object      = 8,
        string      = 9
    };

    struct Writer
    {
        std::vector<uint8_t> data;

        void writeType (const Type& t)
        {
            switch (t.mainType)
            {
                case MainType::int32:           writeType (EncodedType::int32); break;
                case MainType::int64:           writeType (EncodedType::int64); break;
                case MainType::float32:         writeType (EncodedType::float32); break;
                case MainType::float64:         writeType (EncodedType::float64); break;
                case MainType::boolean:         writeType (EncodedType::boolean); break;
                case MainType::string:          writeType (EncodedType::string); break;

                case MainType::vector:          return writeVector (t.content.vector);
                case MainType::primitiveArray:  return writeArray (t.content.primitiveArray);
                case MainType::complexArray:    return writeArray (*t.content.complexArray);
                case MainType::object:          return writeObject (*t.content.object);

                case MainType::void_:
                default:                        throwError ("Invalid type");
            }
        }

    private:
        void writeVector (const Vector& v)
        {
            writeType (EncodedType::vector);
            writeInt (v.numElements);
            writeType (Type (v.elementType));
        }

        void writeArray (const PrimitiveArray& a)
        {
            writeType (EncodedType::array);

            if (a.numElements == 0)
            {
                writeInt (0);
            }
            else
            {
                writeInt (1u);
                writeInt (a.numElements);
                writeType (a.getElementType());
            }
        }

        void writeArray (const ComplexArray& a)
        {
            writeType (EncodedType::array);
            writeInt (static_cast<uint32_t> (a.groups.size()));

            for (auto& g : a.groups)
            {
                writeInt (g.repetitions);
                writeType (g.elementType);
            }
        }

        void writeObject (const Object& o)
        {
            writeType (EncodedType::object);
            writeInt (static_cast<uint32_t> (o.members.size()));
            writeString (o.className);

            for (auto& m : o.members)
            {
                writeType (m.type);
                writeString (m.name);
            }
        }

        void writeType (EncodedType t)            { writeByte (static_cast<uint8_t> (t)); }
        void writeByte (uint8_t byte)             { data.push_back (byte); }
        void writeString (const std::string& s)   { for (auto c : s) writeByte (static_cast<uint8_t> (c)); writeByte (0); }

        void writeInt (uint32_t value)
        {
            while (value > 127)
            {
                writeByte (static_cast<uint8_t> ((value & 0x7fu) | 0x80u));
                value >>= 7;
            }

            writeByte (static_cast<uint8_t> (value));
        }
    };

    struct Reader
    {
        const uint8_t* data;
        const uint8_t* end;

        Type readSingleType()
        {
            auto t = readType();
            expect (data == end);
            return t;
        }

    private:
        [[noreturn]] void throwDataError()      { throwError ("Malformed data"); }
        void expect (bool condition)            { if (! condition) throwDataError(); }

        Type readType()
        {
            switch (static_cast<EncodedType> (readByte()))
            {
                case EncodedType::void_:     return {};
                case EncodedType::int32:     return createInt32();
                case EncodedType::int64:     return createInt64();
                case EncodedType::float32:   return createFloat32();
                case EncodedType::float64:   return createFloat64();
                case EncodedType::boolean:   return createBool();
                case EncodedType::string:    return createString();
                case EncodedType::vector:    return readVector();
                case EncodedType::array:     return readArray();
                case EncodedType::object:    return readObject();
                default:                     throwDataError();
            }
        }

        Type readVector()
        {
            auto num = readInt();
            expect (num <= maxNumVectorElements);

            switch (static_cast<EncodedType> (readByte()))
            {
                case EncodedType::int32:      return Type (MainType::int32, num);
                case EncodedType::int64:      return Type (MainType::int64, num);;
                case EncodedType::float32:    return Type (MainType::float32, num);;
                case EncodedType::float64:    return Type (MainType::float64, num);;
                case EncodedType::boolean:    return Type (MainType::boolean, num);;
                case EncodedType::string:
                case EncodedType::vector:
                case EncodedType::array:
                case EncodedType::object:
                case EncodedType::void_:
                default:                      throwDataError();
            }
        }

        Type readArray()
        {
            auto t = createEmptyArray();
            auto numGroups = readInt();
            uint32_t elementCount = 0;

            for (uint32_t i = 0; i < numGroups; ++i)
            {
                auto numReps = readInt();
                expect (numReps <= maxNumArrayElements - elementCount);
                elementCount += numReps;
                t.addArrayElements (readType(), numReps);
            }

            return t;
        }

        Type readObject()
        {
            auto numMembers = readInt();
            auto t = createObject (readString());

            for (uint32_t i = 0; i < numMembers; ++i)
            {
                auto memberType = readType();
                t.addObjectMember (readString(), std::move (memberType));
            }

            return t;
        }

        uint8_t readByte()
        {
            expect (data < end);
            return *data++;
        }

        uint32_t readInt()
        {
            uint32_t result = 0;

            for (int shift = 0;;)
            {
                auto nextByte = readByte();

                if (shift == 28)
                    expect (nextByte < 16);

                if (nextByte < 128)
                    return result | (static_cast<uint32_t> (nextByte) << shift);

                result |= static_cast<uint32_t> (nextByte & 0x7fu) << shift;
                shift += 7;
            }
        }

        std::string readString()
        {
            auto start = reinterpret_cast<const char*> (data);

            for (;;)
                if (readByte() == 0)
                    return std::string (start, reinterpret_cast<const char*> (data - 1));
        }
    };
};

inline std::vector<uint8_t> Type::serialise() const
{
    SerialisationHelpers::Writer w;
    w.writeType (*this);
    return std::move (w.data);
}

inline Type Type::deserialise (const void* dataStart, const void* dataEnd)
{
    SerialisationHelpers::Reader r { static_cast<const uint8_t*> (dataStart), static_cast<const uint8_t*> (dataEnd) };
    return r.readSingleType();
}

//==============================================================================
inline ValueView::ValueView() = default;
inline ValueView::ValueView (StringDictionary& dic) : stringDictionary (std::addressof (dic)) {}
inline ValueView::ValueView (Type&& t, void* d, StringDictionary* dic) : type (std::move (t)), data (static_cast<uint8_t*> (d)), stringDictionary (dic) {}
inline ValueView::ValueView (const Type& t, void* d, StringDictionary* dic) : type (t), data (static_cast<uint8_t*> (d)), stringDictionary (dic) {}

template <typename ElementType>
ValueView ValueView::createArray (ElementType* targetData, uint32_t numElements)
{
    static_assert (isValidPrimitiveType<ElementType>(), "The template type needs to be one of the supported primitive types");
    return ValueView (Type::createArray<ElementType> (numElements), targetData, nullptr);
}

template <typename ElementType>
ValueView ValueView::create2DArray (ElementType* sourceData, uint32_t numArrayElements, uint32_t numVectorElements)
{
    static_assert (isValidPrimitiveType<ElementType>(), "The template type needs to be one of the supported primitive types");
    return ValueView (Type::createArrayOfVectors<ElementType> (numArrayElements, numVectorElements), sourceData, nullptr);
}

template <typename TargetType>
TargetType ValueView::readContentAs() const
{
    TargetType v;
    memcpy (std::addressof (v), data, sizeof (v));
    return v;
}

template <typename TargetType> TargetType ValueView::readPrimitive (Type::MainType t) const
{
    switch (t)
    {
        case Type::MainType::int32:       return static_cast<TargetType> (readContentAs<int32_t>());
        case Type::MainType::int64:       return static_cast<TargetType> (readContentAs<int64_t>());
        case Type::MainType::float32:     return static_cast<TargetType> (readContentAs<float>());
        case Type::MainType::float64:     return static_cast<TargetType> (readContentAs<double>());
        case Type::MainType::boolean:     return static_cast<TargetType> (readContentAs<uint8_t>() != 0);

        case Type::MainType::vector:
        case Type::MainType::string:
        case Type::MainType::primitiveArray:
        case Type::MainType::complexArray:
        case Type::MainType::object:
        case Type::MainType::void_:
        default:                          throwError ("Cannot convert this value to a numeric type");
    }
}

inline int32_t  ValueView::getInt32() const     { check (type.isInt32(),   "Value is not an int32");   return readContentAs<int32_t>(); }
inline int64_t  ValueView::getInt64() const     { check (type.isInt64(),   "Value is not an int64");   return readContentAs<int64_t>(); }
inline float    ValueView::getFloat32() const   { check (type.isFloat32(), "Value is not a float32");  return readContentAs<float>(); }
inline double   ValueView::getFloat64() const   { check (type.isFloat64(), "Value is not a float64");  return readContentAs<double>(); }
inline bool     ValueView::getBool() const      { check (type.isBool(),    "Value is not a bool");     return readContentAs<uint8_t>() != 0; }

template <typename TargetType> TargetType ValueView::get() const
{
    if constexpr (matchesType<TargetType, std::string, std::string_view>())
    {
        return TargetType (getString());
    }
    else if constexpr (matchesType<TargetType, uint32_t, uint64_t>())
    {
        using SignedType = typename std::make_signed<TargetType>::type;
        auto n = get<SignedType>();
        check (n >= 0, "Value out of range");
        return static_cast<TargetType> (n);
    }
    else
    {
        static_assert (isValidPrimitiveType<TargetType>(), "The TargetType template argument must be a valid primitive type");
        return readPrimitive<TargetType> (type.isVectorSize1() ? type.content.vector.elementType
                                                               : type.mainType);
    }
}

template <typename TargetType> TargetType ValueView::getWithDefault (TargetType defaultValue) const
{
    if constexpr (matchesType<TargetType, std::string, std::string_view>())
    {
        if (isString())
            return TargetType (getString());
    }
    else
    {
        static_assert (isValidPrimitiveType<TargetType>() || matchesType<TargetType, uint32_t, uint64_t>(),
                       "The TargetType template argument must be a valid primitive type");

        if (type.isPrimitive())     return readPrimitive<TargetType> (type.mainType);
        if (type.isVectorSize1())   return readPrimitive<TargetType> (type.content.vector.elementType);
    }

    return defaultValue;
}

template <typename PrimitiveType> void ValueView::setUnchecked (PrimitiveType v)
{
    static_assert (isValidPrimitiveType<PrimitiveType>() || matchesType<PrimitiveType, std::string&, std::string_view, const char*>(),
                   "The template type needs to be one of the supported primitive types");

    if constexpr (matchesType<PrimitiveType, bool>())                       { *data = v ? 1 : 0; return; }
    if constexpr (matchesType<PrimitiveType, StringDictionary::Handle>())   return setUnchecked (static_cast<int32_t> (v.handle));

    if constexpr (matchesType<PrimitiveType, std::string, std::string_view, const char*>())
    {
        check (stringDictionary != nullptr, "No string dictionary supplied");
        return setUnchecked (stringDictionary->getHandleForString (v));
    }

    memcpy (data, std::addressof (v), sizeof (v));
}

template <typename PrimitiveType> void ValueView::set (PrimitiveType v)
{
    static_assert (isValidPrimitiveType<PrimitiveType>() || matchesType<PrimitiveType, std::string&, std::string_view, const char*>(),
                   "The template type needs to be one of the supported primitive types");

    if constexpr (matchesType<PrimitiveType, int32_t>())  check (type.isInt32(),   "Value is not an int32");;
    if constexpr (matchesType<PrimitiveType, int64_t>())  check (type.isInt64(),   "Value is not an int64");;
    if constexpr (matchesType<PrimitiveType, float>())    check (type.isFloat32(), "Value is not a float32");
    if constexpr (matchesType<PrimitiveType, double>())   check (type.isFloat64(), "Value is not a float64");
    if constexpr (matchesType<PrimitiveType, bool>())     check (type.isBool(),    "Value is not a bool");

    if constexpr (matchesType<PrimitiveType, StringDictionary::Handle, std::string&, std::string_view, const char*>())
        check (type.isString(),  "Value is not a string");

    setUnchecked (v);
}

inline std::string_view ValueView::getString() const
{
    check (type.isString(), "Value is not a string");
    auto handle = StringDictionary::Handle { readContentAs<decltype (StringDictionary::Handle::handle)>() };
    check (stringDictionary != nullptr, "No string dictionary supplied");
    return stringDictionary->getStringForHandle (handle);
}

inline uint32_t ValueView::size() const             { return type.getNumElements(); }

inline ValueView ValueView::operator[] (uint32_t index) const
{
    check (isVector() || isArray() || isObject(), "This object is not an array or vector");

    auto info = type.getElementTypeAndOffset (index);
    return ValueView (std::move (info.elementType), data + info.offset, stringDictionary);
}

inline ValueView ValueView::operator[] (int index) const          { return operator[] (static_cast<uint32_t> (index)); }
inline ValueView ValueView::operator[] (const char* name) const   { return operator[] (std::string_view (name)); }

inline ValueView ValueView::operator[] (std::string_view name) const
{
    auto index = type.getObjectMemberIndex (name);

    if (index < 0)
        return {};

    auto info = type.getElementTypeAndOffset (static_cast<uint32_t> (index));
    return ValueView (std::move (info.elementType), data + info.offset, stringDictionary);
}

inline const std::string& ValueView::getObjectClassName() const   { return type.getObjectClassName(); }

inline MemberNameAndValue ValueView::getObjectMemberAt (uint32_t index) const
{
    auto& member = type.getObjectMember (index);
    auto info = type.getElementTypeAndOffset (index);
    return { member.name.c_str(), ValueView (std::move (info.elementType), data + info.offset, stringDictionary) };
}

inline bool ValueView::hasObjectMember (std::string_view name) const
{
    return type.getObjectMemberIndex (name) >= 0;
}

template <typename Visitor>
void ValueView::visitObjectMembers (Visitor&& visit) const
{
    check (isObject(), "This value is not an object");
    auto numMembers = size();

    for (uint32_t i = 0; i < numMembers; ++i)
    {
        auto& member = type.getObjectMember (i);
        auto info = type.getElementTypeAndOffset (i);
        visit (MemberNameAndValue { member.name.c_str(), ValueView (std::move (info.elementType), data + info.offset, stringDictionary) });
    }
}

struct ValueView::Iterator
{
    Iterator (const Iterator&) = default;
    Iterator& operator= (const Iterator&) = default;

    ValueView operator*() const             { return value[index]; }
    Iterator& operator++()                  { ++index; return *this; }
    Iterator operator++ (int)               { auto old = *this; ++*this; return old; }
    bool operator== (EndIterator) const     { return index == numElements; }
    bool operator!= (EndIterator) const     { return index != numElements; }

    ValueView value;
    uint32_t index, numElements;
};

inline ValueView::Iterator ValueView::begin() const   { return { *this, 0, size() }; }


//==============================================================================
inline Value::Value() : value (dictionary) {}

inline Value::Value (Value&& other)
   : packedData (std::move (other.packedData)), dictionary (std::move (other.dictionary)),
     value (std::move (other.value.type), packedData.data(), std::addressof (dictionary))
{
}

inline Value::Value (const Value& other)
   : packedData (other.packedData), dictionary (other.dictionary),
     value (other.value.type, packedData.data(), std::addressof (dictionary))
{
}

inline Value& Value::operator= (Value&& other)
{
    packedData = std::move (other.packedData);
    dictionary = std::move (other.dictionary);
    value.type = std::move (other.value.type);
    value.data = packedData.data();
    return *this;
}

inline Value& Value::operator= (const Value& other)
{
    packedData = other.packedData;
    dictionary = other.dictionary;
    value.type = other.value.type;
    value.data = packedData.data();
    return *this;
}

inline Value::Value (const Type& t)
   : packedData (static_cast<std::vector<char>::size_type> (t.getValueDataSize())),
     value (t, packedData.data(), std::addressof (dictionary))
{
}

inline Value::Value (Type&& t)
   : packedData (static_cast<std::vector<char>::size_type> (t.getValueDataSize())),
     value (std::move (t), packedData.data(), std::addressof (dictionary))
{
}

inline Value::Value (const ValueView& source)  : Value (source.getType())
{
    memcpy (getRawData(), source.getRawData(), getRawDataSize());

    if (source.stringDictionary != nullptr && source.getType().usesStrings())
        importStringHandles (value, *source.stringDictionary);
}

inline Value& Value::operator= (const ValueView& source)
{
    packedData.resize (source.getType().getValueDataSize());
    value.type = source.type;
    value.data = packedData.data();
    memcpy (value.data, source.getRawData(), getRawDataSize());
    dictionary.strings.clear();

    if (source.stringDictionary != nullptr && source.getType().usesStrings())
        importStringHandles (value, *source.stringDictionary);

    return *this;
}

inline void Value::appendValue (ValueView newValue)
{
    check (! newValue.isVoid(), "Cannot add a void value");

    auto source = static_cast<const uint8_t*> (newValue.getRawData());
    auto oldSize = packedData.size();
    packedData.insert (packedData.end(), source, source + newValue.getType().getValueDataSize());
    value.data = packedData.data();

    if (auto oldDictionary = newValue.stringDictionary)
    {
        if (newValue.getType().usesStrings())
        {
            newValue.data = packedData.data() + oldSize;
            newValue.stringDictionary = std::addressof (dictionary);
            importStringHandles (newValue, *oldDictionary);
        }
    }
}

inline void Value::importStringHandles (ValueView& target, const StringDictionary& oldDictionary)
{
    struct StringHandleImporter
    {
        const StringDictionary& oldDic;

        void importStrings (ValueView& v)
        {
            if (v.getType().usesStrings())
            {
                if (v.isString())
                {
                    auto oldHandle = StringDictionary::Handle { v.readContentAs<decltype (StringDictionary::Handle::handle)>() };
                    v.setUnchecked (v.stringDictionary->getHandleForString (oldDic.getStringForHandle (oldHandle)));
                }
                else if (v.isArray())
                {
                    for (auto element : v)
                        importStrings (element);
                }
                else if (v.isObject())
                {
                    auto numMembers = v.size();

                    for (uint32_t i = 0; i < numMembers; ++i)
                    {
                        auto member = v[i];
                        importStrings (member);
                    }
                }
            }
        }
    };

    StringHandleImporter { oldDictionary }.importStrings (target);
}

inline Value Value::createPrimitive (int32_t n)         { Value v (Type::createInt32());   v.value.setUnchecked (n); return v; }
inline Value Value::createPrimitive (int64_t n)         { Value v (Type::createInt64());   v.value.setUnchecked (n); return v; }
inline Value Value::createPrimitive (float n)           { Value v (Type::createFloat32()); v.value.setUnchecked (n); return v; }
inline Value Value::createPrimitive (double n)          { Value v (Type::createFloat64()); v.value.setUnchecked (n); return v; }
inline Value Value::createPrimitive (bool n)            { Value v (Type::createBool());    v.value.setUnchecked (n); return v; }
inline Value Value::createString (std::string_view s)   { Value v (Type::createString());  v.value.setUnchecked (v.dictionary.getHandleForString (s)); return v; }

inline Value Value::createInt32   (int32_t v)     { return createPrimitive (v); }
inline Value Value::createInt64   (int64_t v)     { return createPrimitive (v); }
inline Value Value::createFloat32 (float v)       { return createPrimitive (v); }
inline Value Value::createFloat64 (double v)      { return createPrimitive (v); }
inline Value Value::createBool    (bool v)        { return createPrimitive (v); }

inline Value Value::createEmptyArray()            { return Value (Type::createEmptyArray()); }

template <typename ElementType>
inline Value Value::createVector (const ElementType* source, uint32_t numElements)
{
    static_assert (ValueView::isValidPrimitiveType<ElementType>(), "The template type needs to be one of the supported primitive types");
    Value v (Type::createVector<ElementType> (numElements));
    memcpy (v.getRawData(), source, sizeof (ElementType) * numElements);
    return v;
}

template <typename GetElementValue>
inline Value Value::createVector (uint32_t numElements, const GetElementValue& getValueForIndex)
{
    using ElementType = decltype (getValueForIndex (0));
    static_assert (ValueView::isValidPrimitiveType<ElementType>(), "The template type needs to be one of the supported primitive types");
    Value v (Type::createVector<ElementType> (numElements));

    for (uint32_t i = 0; i < numElements; ++i)
        static_cast<ElementType*> (v.getRawData())[i] = getValueForIndex (i);

    return v;
}

template <typename GetElementValue>
inline Value Value::createArray (uint32_t numElements, const GetElementValue& getValueForIndex)
{
    using ElementType = decltype (getValueForIndex (0));
    static_assert (ValueView::isValidPrimitiveType<ElementType>() || ValueView::matchesType<ElementType, ValueView>(),
                   "The functor needs to return either a supported primitive type, or a Value");

    if constexpr (ValueView::isValidPrimitiveType<ElementType>())
    {
        Value v (Type::createArray (numElements, Type::createPrimitive<ElementType>()));

        for (uint32_t i = 0; i < numElements; ++i)
            static_cast<ElementType*> (v.getRawData())[i] = getValueForIndex (i);

        return v;
    }
    else
    {
        Value v (Type::createEmptyArray());

        for (uint32_t i = 0; i < numElements; ++i)
        {
            ValueView element (getValueForIndex (i));
            v.value.type.addArrayElements (element.type, 1);
            v.appendValue (element);
        }

        return v;
    }
}

template <typename GetElementValue>
inline Value Value::createArray (uint32_t numArrayElements, uint32_t numVectorElements, const GetElementValue& getValueAt)
{
    using ElementType = decltype (getValueAt (0, 0));
    static_assert (ValueView::isValidPrimitiveType<ElementType>(), "The functor needs to return a supported primitive type");

    Value v (Type::createArray (numArrayElements, Type::createVector<ElementType> (numVectorElements)));
    auto dest = static_cast<ElementType*> (v.getRawData());

    for (uint32_t j = 0; j < numArrayElements; ++j)
        for (uint32_t i = 0; i < numVectorElements; ++i)
            *dest++ = getValueAt (j, i);

    return v;
}

template <typename ElementType>
Value Value::create2DArray (const ElementType* sourceData, uint32_t numArrayElements, uint32_t numVectorElements)
{
    static_assert (ValueView::isValidPrimitiveType<ElementType>(), "The template type needs to be one of the supported primitive types");
    Value v (Type::createArrayOfVectors<ElementType> (numArrayElements, numVectorElements));
    std::copy_n (sourceData, numArrayElements * numVectorElements, static_cast<ElementType*> (v.getRawData()));
    return v;
}

inline void Value::addArrayElement (const ValueView& v)
{
    value.type.addArrayElements (v.getType(), 1);
    appendValue (v);
}

inline Value Value::createObject (std::string className)
{
    return Value (Type::createObject (std::move (className)));
}

inline void Value::addObjectMember (std::string memberName, const ValueView& memberValue)
{
    value.type.addObjectMember (std::move (memberName), memberValue.getType());
    appendValue (memberValue);
}

template <typename TargetType> TargetType Value::get() const                           { return value.get<TargetType>(); }
template <typename TargetType> TargetType Value::getWithDefault (TargetType d) const   { return value.getWithDefault<TargetType> (std::forward (d)); }

inline ValueView::Iterator Value::begin() const    { return value.begin(); }
inline ValueView::EndIterator Value::end() const   { return {}; }

inline Value::SimpleStringDictionary::Handle Value::SimpleStringDictionary::getHandleForString (std::string_view text)
{
    if (text.empty())
        return {};

    for (decltype(Handle::handle) i = 0; i < strings.size(); ++i)
        if (strings[i] == text)
            return { i + 1 };

    strings.push_back (std::string (text));
    return { static_cast<decltype(Handle::handle)> (strings.size()) };
}

inline std::string_view Value::SimpleStringDictionary::getStringForHandle (Handle handle) const
{
    if (handle == Handle())
        return {};

    if (handle.handle <= strings.size())
        return strings[handle.handle - 1];

    throwError ("Unknown string");
}


} // namespace choc::value

#endif // CHOC_VALUE_POOL_HEADER_INCLUDED
