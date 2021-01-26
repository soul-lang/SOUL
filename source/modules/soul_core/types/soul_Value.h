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

//==============================================================================
/**
    Holds any constant value that can be represented by the Type class.

    This can be any value that SOUL can represent - structures, arrays, vectors,
    etc, including nested types.

    Value can be passed by-value and for simple types will be pretty lightweight without
    any heap storage, but obviously since it could contain megabytes of structured data
    you should be a little wary of copying these too freely.
*/
struct Value  final
{
    /** Default-constructing a Value will give you an object for which many methods are
        not permitted (and will assert), and which isValid() returns false.
    */
    Value();

    ~Value();

    Value (const Value&);
    Value& operator= (const Value&);
    Value (Value&&);
    Value& operator= (Value&&);

    explicit Value (int32_t);
    explicit Value (int64_t);
    explicit Value (float);
    explicit Value (double);
    explicit Value (bool);
    explicit Value (std::complex<float>);
    explicit Value (std::complex<double>);

    static Value zeroInitialiser (Type);
    static Value createArrayOrVector (Type arrayOrVectorType, ArrayView<Value> elements);
    static Value createStruct (Structure&, ArrayView<Value> members);
    static Value createStringLiteral (StringDictionary::Handle);
    static Value createUnsizedArray (const Type& elementType, ConstantTable::Handle);
    static Value createFromRawData (Type type, const void* data, size_t dataSize);

    /** Creates an array of float vectors to match the size of the data provided. */
    static Value createFloatVectorArray (choc::buffer::InterleavedView<float> data);
    static Value createFloatVectorArray (choc::buffer::ChannelArrayView<float> data);

    template <typename IntType>
    static Value createInt32 (IntType n)           { return Value (static_cast<int32_t> (n)); }

    template <typename IntType>
    static Value createInt64 (IntType n)           { return Value (static_cast<int64_t> (n)); }

    template <typename IntType>
    static Value createArrayIndex (IntType n)      { return createInt32 (Type::castToArraySize (n)); }

    bool    getAsBool() const;
    float   getAsFloat() const;
    double  getAsDouble() const;
    int32_t getAsInt32() const;
    int64_t getAsInt64() const;
    std::complex<float>  getAsComplex32() const;
    std::complex<double> getAsComplex64() const;
    StringDictionary::Handle getStringLiteral() const;
    ConstantTable::Handle getUnsizedArrayContent() const;

    explicit operator float() const;
    explicit operator double() const;
    explicit operator int32_t() const;
    explicit operator int64_t() const;
    explicit operator bool() const;

    /** Returns the type of the value */
    const Type& getType() const;

    /** Provides a writable reference to the type in case a responsible adult needs to modify it. */
    Type& getMutableType();

    /** Returns a readable description of the value. For more control over the format, see ValuePrinter. */
    std::string getDescription (const StringDictionary* dictionary = nullptr) const;

    /** Uses the given ValuePrinter to print this value. */
    void print (ValuePrinter&) const;

    /** Internally the entire value (including all nested objects) is stored as a continguous packed chunk of
        memory - this provides access to it with all the dangers that entails.
    */
    void* getPackedData() const                    { return allocatedData.data(); }

    /** The total size of the packed data which fully represents this object. */
    size_t getPackedDataSize() const               { return allocatedData.size(); }

    /** Assuming the value is an array of float32 primitives or vectors, this returns a channel set
        which points directly into the packed data.
    */
    choc::buffer::InterleavedView<float> getAsChannelSet32() const;

    /** Assuming the value is an array of float64 primitives or vectors, this returns a channel set
        which points directly into the packed data.
    */
    choc::buffer::InterleavedView<double> getAsChannelSet64() const;

    /** True if this Value is not an uninitialised (i.e. default-constructed) value. */
    bool isValid() const;

    /** If this value (or all its internal elements if it's an aggregate type) is zero. */
    bool isZero() const;

    /** Resets this value to a zero-initialised version of its type. */
    void clear();

    /** Attempts to cast to a new type, returning an invalid Value on failure. */
    Value tryCastToType (const Type& destType) const;
    /** Attempts to cast to a new type, returning an invalid Value and an error message on failure. */
    Value tryCastToType (const Type& destType, CompileMessage& errorMessage) const;
    /** Attempts to cast to a new type, triggering an internal compiler error on failure. */
    Value castToTypeExpectingSuccess (const Type& destType) const;

    template <typename Thrower>
    Value castToTypeWithError (const Type& destType, Thrower&& errorLocation) const
    {
        CompileMessage errorMessage;
        auto result = tryCastToType (destType, errorMessage);

        if (! result.isValid())
            errorLocation.throwError (errorMessage);

        return result;
    }

    Value cloneWithEquivalentType (Type) const;

    Value getSubElement (const SubElementPath&) const;

    void modifySubElementInPlace (const SubElementPath&, const Value& newValue);
    void modifySubElementInPlace (const SubElementPath&, const void* newData);

    Value getSlice (size_t start, size_t end) const;

    void modifyArraySizeInPlace (size_t newSize);

    bool canNegate() const;
    Value negated() const;

    void convertAllHandlesToPointers (ConstantTable&);

    bool operator== (const Value&) const;
    bool operator!= (const Value&) const;

    /** Copies the value from the source value - this is only valid if the type are identical. */
    void copyValue (const Value& source);

    static Value fromExternalValue (const Type& targetType, const choc::value::ValueView&, ConstantTable&, StringDictionary&);
    choc::value::Value toExternalValue (const ConstantTable&, const StringDictionary&) const;

private:
    Type type;

    class LazyAllocatedData
    {
    public:
        LazyAllocatedData (size_t size = 0) : allocatedSize (size) {}

        uint8_t* data() const
        {
            if (allocatedData.size() == 0)
                allocatedData.resize (allocatedSize);

            return allocatedData.data();
        }

        size_t size() const
        {
            return allocatedSize;
        }

        bool isAllocated() const
        {
            return allocatedData.size() != 0;
        }

    private:
        size_t allocatedSize = 0;
        mutable ArrayWithPreallocation<uint8_t, 8> allocatedData;
    };

    LazyAllocatedData allocatedData;

    struct PackedData;
    PackedData getData() const;

    Value (Type);
    Value (Type, const void*);
};

//==============================================================================
struct ValuePrinter
{
    ValuePrinter();
    virtual ~ValuePrinter();

    virtual void printInt32 (int32_t);
    virtual void printInt64 (int64_t);
    virtual void printFloat32 (float);
    virtual void printFloat64 (double);
    virtual void printBool (bool);

    virtual void printZeroInitialiser (const Type&);

    virtual void beginStructMembers (const Type&);
    virtual void printStructMemberSeparator();
    virtual void endStructMembers();

    virtual void beginArrayMembers (const Type&);
    virtual void printArrayMemberSeparator();
    virtual void endArrayMembers();

    virtual void beginVectorMembers (const Type&);
    virtual void printVectorMemberSeparator();
    virtual void endVectorMembers();

    virtual void printStringLiteral (StringDictionary::Handle);
    virtual void printUnsizedArrayContent (const Type& arrayType, const void*);

    virtual void print (std::string_view) = 0;

    const StringDictionary* dictionary = nullptr;
};


} // namespace soul
