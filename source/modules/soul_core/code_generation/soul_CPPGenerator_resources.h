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

namespace soul::cpp
{

static constexpr auto headerComment = R"cppcode(
//      _____ _____ _____ __
//     |   __|     |  |  |  |        Auto-generated C++
//     |__   |  |  |  |  |  |__      SOUL Version VERSION
//     |_____|_____|_____|_____|     https://soul.dev
//
)cppcode";

//==============================================================================
static constexpr auto standardIncludes = R"cppcode(

#if ! ((defined (__cplusplus) && (__cplusplus >= 201703L)) || (defined (_MSVC_LANG) && (_MSVC_LANG >= 201703L)))
 #error The SOUL auto-generated code requires a C++17 compiler
#endif

#include <array>
#include <functional>
#include <cmath>
#include <cstddef>
#include <limits>
#include <cstring>
)cppcode";

//==============================================================================
static constexpr auto juceHeaderGuard = R"cppcode(
#ifndef JUCE_AUDIO_PROCESSORS_H_INCLUDED
 #error "This file is designed to be included inside a file in a JUCE project, so that the module headers have already been included before it"
#endif
)cppcode";

static constexpr auto matchingHeaderGuard = R"cppcode(
#ifndef HEADER_HASH
 #error "This file is designed to be included inside a file where its corresponding auto-generated header has already been included"
#endif
)cppcode";

//==============================================================================
static constexpr auto definitions = R"cppcode(
#ifndef SOUL_CPP_ASSERT
 #define SOUL_CPP_ASSERT(x)
#endif

// If you need to provide custom implementations of the instrinsics that soul uses,
// you can set this macro to provide your own namespace containing them.
#ifndef SOUL_INTRINSICS
 #define SOUL_INTRINSICS std
#endif

)cppcode";

//==============================================================================
static constexpr auto forwardDecls = R"cppcode(
template <typename Type, int32_t size> struct Vector;
template <typename Type, int32_t size> struct FixedArray;
template <typename Type> struct DynamicArray;

static constexpr uint32_t maxBlockSize  = MAX_BLOCK_SIZE;
static constexpr uint32_t latency       = LATENCY;

template <typename Item>
struct span
{
    Item* start = nullptr;
    size_t numItems = 0;

    constexpr size_t size() const               { return numItems; }
    constexpr bool empty() const                { return numItems == 0; }
    constexpr Item* begin() const               { return start; }
    constexpr Item* end() const                 { return start + numItems; }
    const Item& operator[] (size_t index) const { SOUL_CPP_ASSERT (index < numItems); return start[index]; }
};

)cppcode";

//==============================================================================
static constexpr auto essentialMethods = R"cppcode(
void init (double newSampleRate, int sessionID)
{
    memset (reinterpret_cast<void*> (std::addressof (state)), 0, sizeof (state));
    sampleRate = newSampleRate;
    _initialise (state, sessionID);
    initialisedState = state;
}

void reset() noexcept
{
    state = initialisedState;
}

)cppcode";

//==============================================================================
static constexpr auto prepareAndAdvanceMethods = R"cppcode(
void prepare (uint32_t numFramesToBeRendered)
{
    SOUL_CPP_ASSERT (numFramesToBeRendered != 0);
    framesToAdvance = numFramesToBeRendered;
    _prepare (state, static_cast<int32_t> (numFramesToBeRendered));
}

void advance()
{
    SOUL_CPP_ASSERT (framesToAdvance != 0); // you must call prepare() before advance()!
    auto framesRemaining = framesToAdvance;

    while (framesRemaining > 0)
    {
        auto framesThisCall = framesRemaining < maxBlockSize ? framesRemaining : maxBlockSize;

        run (state, static_cast<int32_t> (framesThisCall));

        totalFramesElapsed += framesThisCall;
        framesRemaining -= framesThisCall;
    }
}

)cppcode";

//==============================================================================
static constexpr auto warningsPush = R"cppcode(
#if __clang__
 #pragma clang diagnostic push
 #pragma clang diagnostic ignored "-Wunused-label"
 #pragma clang diagnostic ignored "-Wunused-parameter"
 #pragma clang diagnostic ignored "-Wshadow"
#elif defined(__GNUC__)
 #pragma GCC diagnostic push
 #pragma GCC diagnostic ignored "-Wunused-label"
 #pragma GCC diagnostic ignored "-Wunused-parameter"
 #pragma GCC diagnostic ignored "-Wshadow"
#elif defined(_MSC_VER)
 #pragma warning (push)
 #pragma warning (disable: 4100 4102 4458)
#endif
)cppcode";

//==============================================================================
static constexpr auto warningsPop = R"cppcode(
#if __clang__
 #pragma clang diagnostic pop
#elif defined(__GNUC__)
 #pragma GCC diagnostic pop
#elif defined(_MSC_VER)
 #pragma warning (pop)
#endif
)cppcode";

//==============================================================================
static constexpr auto memberVariables = R"cppcode(
_State state = {}, initialisedState;

double sampleRate = 1.0;
uint32_t framesToAdvance = 0;
uint64_t totalFramesElapsed = 0;
)cppcode";


//==============================================================================
static constexpr auto renderHelperClasses = R"cppcode(
static constexpr uint32_t numAudioInputChannels  = NUM_AUDIO_IN_CHANS;
static constexpr uint32_t numAudioOutputChannels = NUM_AUDIO_OUT_CHANS;

struct MIDIMessage
{
    uint32_t frameIndex = 0;
    uint8_t byte0 = 0, byte1 = 0, byte2 = 0;
};

struct MIDIMessageArray
{
    const MIDIMessage* messages = nullptr;
    uint32_t numMessages = 0;
};

template <typename FloatType = float>
struct RenderContext
{
    std::array<const FloatType*, NUM_AUDIO_IN_CHANS> inputChannels;
    std::array<FloatType*, NUM_AUDIO_OUT_CHANS> outputChannels;
    MIDIMessageArray  incomingMIDI;
    uint32_t          numFrames = 0;
};
)cppcode";

//==============================================================================
static constexpr auto renderMIDIPreamble = R"cppcode(
auto endMIDIIndex = startMIDIIndex;

while (endMIDIIndex < context.incomingMIDI.numMessages)
{
    auto eventTime = context.incomingMIDI.messages[endMIDIIndex].frameIndex;

    if (eventTime > startFrame)
    {
        auto framesUntilEvent = eventTime - startFrame;

        if (framesUntilEvent < numFramesToDo)
            numFramesToDo = framesUntilEvent;

        break;
    }

    ++endMIDIIndex;
}

prepare (numFramesToDo);

while (startMIDIIndex < endMIDIIndex)
)cppcode";

//==============================================================================
static constexpr auto helperClasses = R"cppcode(
struct ZeroInitialiser
{
    template <typename Type>   operator Type() const   { return {}; }
    template <typename Index>  ZeroInitialiser operator[] (Index) const { return {}; }
};

//==============================================================================
template <typename Type>
struct DynamicArray
{
    using ElementType = Type;
    ElementType* elements = nullptr;
    int32_t numElements = 0;

    constexpr ElementType& operator[] (int i) noexcept                   { return elements[i]; }
    constexpr const ElementType& operator[] (int i) const noexcept       { return elements[i]; }
    constexpr int getElementSizeBytes() const noexcept                   { return sizeof (ElementType); }
};

//==============================================================================
template <typename Type, int32_t size>
struct FixedArray
{
    using ElementType = Type;
    ElementType elements[size];
    static constexpr int32_t numElements = size;

    static constexpr FixedArray fromRepeatedValue (ElementType value)
    {
        FixedArray a;

        for (auto& element : a.elements)
            element = value;

        return a;
    }

    static size_t elementOffset (int i) noexcept               { return sizeof (ElementType) * i; }
    ElementType& operator[] (int i) noexcept                   { return elements[i]; }
    const ElementType& operator[] (int i) const noexcept       { return elements[i]; }
    int getElementSizeBytes() const noexcept                   { return sizeof (ElementType); }
    DynamicArray<ElementType> toDynamicArray() const noexcept  { return { const_cast<ElementType*> (&elements[0]), size }; }
    operator ElementType*() const noexcept                     { return const_cast<ElementType*> (&elements[0]); }

    FixedArray& operator= (ZeroInitialiser)
    {
        for (auto& e : elements)
            e = ElementType {};

        return *this;
    }

    template <int start, int end>
    constexpr FixedArray<Type, end - start> slice() const noexcept
    {
        FixedArray<Type, end - start> newSlice;

        for (int i = 0; i < end - start; ++i)
            newSlice.elements[i] = elements[start + i];

        return newSlice;
    }
};

//==============================================================================
template <typename Type, int32_t size>
struct Vector
{
    using ElementType = Type;
    ElementType elements[size] = {};
    static constexpr int32_t numElements = size;

    constexpr Vector() = default;
    constexpr Vector (const Vector&) = default;
    constexpr Vector& operator= (const Vector&) = default;

    explicit constexpr Vector (Type value)
    {
        for (auto& element : elements)
            element = value;
    }

    template <typename OtherType>
    constexpr Vector (const Vector<OtherType, size>& other)
    {
        for (int32_t i = 0; i < size; ++i)
            elements[i] = static_cast<Type> (other.elements[i]);
    }

    constexpr Vector (std::initializer_list<Type> i)
    {
        int n = 0;

        for (auto e : i)
            elements[n++] = e;
    }

    static constexpr Vector fromRepeatedValue (Type value)
    {
        return Vector (value);
    }

    constexpr Vector operator+ (const Vector& rhs) const                { return apply<Vector> (rhs, [] (Type a, Type b) { return a + b; }); }
    constexpr Vector operator- (const Vector& rhs) const                { return apply<Vector> (rhs, [] (Type a, Type b) { return a - b; }); }
    constexpr Vector operator* (const Vector& rhs) const                { return apply<Vector> (rhs, [] (Type a, Type b) { return a * b; }); }
    constexpr Vector operator/ (const Vector& rhs) const                { return apply<Vector> (rhs, [] (Type a, Type b) { return a / b; }); }
    constexpr Vector operator% (const Vector& rhs) const                { return apply<Vector> (rhs, [] (Type a, Type b) { return a % b; }); }
    constexpr Vector operator-() const                                  { return apply<Vector> ([] (Type n) { return -n; }); }
    constexpr Vector operator~() const                                  { return apply<Vector> ([] (Type n) { return ~n; }); }
    constexpr Vector operator!() const                                  { return apply<Vector> ([] (Type n) { return ! n; }); }

    Vector& operator= (ZeroInitialiser)
    {
        for (auto& e : elements)
            e = {};

        return *this;
    }

    constexpr Vector<bool, size> operator== (const Vector& rhs) const   { return apply<Vector<bool, size>> (rhs, [] (Type a, Type b) { return a == b; }); }
    constexpr Vector<bool, size> operator!= (const Vector& rhs) const   { return apply<Vector<bool, size>> (rhs, [] (Type a, Type b) { return a != b; }); }

    template <typename ReturnType, typename Op>
    constexpr ReturnType apply (const Vector& rhs, Op&& op) const noexcept
    {
        ReturnType v;

        for (int i = 0; i < size; ++i)
            v.elements[i] = op (elements[i], rhs.elements[i]);

        return v;
    }

    template <typename ReturnType, typename Op>
    constexpr ReturnType apply (Op&& op) const noexcept
    {
        ReturnType v;

        for (int i = 0; i < size; ++i)
            v.elements[i] = op (elements[i]);

        return v;
    }

    template <int start, int end>
    constexpr Vector<Type, end - start> slice() const noexcept
    {
        Vector<Type, end - start> newSlice;

        for (int i = 0; i < end - start; ++i)
            newSlice.elements[i] = elements[start + i];

        return newSlice;
    }

    constexpr const Type& operator[] (int i) const noexcept  { return elements[i]; }
    constexpr Type& operator[] (int i) noexcept              { return elements[i]; }
};

//==============================================================================
struct StringLiteral
{
    constexpr StringLiteral (int32_t h) noexcept : handle (h) {}
    StringLiteral() = default;
    StringLiteral (const StringLiteral&) = default;
    StringLiteral& operator= (const StringLiteral&) = default;

    const char* toString() const       { return lookupStringLiteral (handle); }
    operator const char*() const       { return lookupStringLiteral (handle); }

    bool operator== (StringLiteral other) const noexcept    { return handle == other.handle; }
    bool operator!= (StringLiteral other) const noexcept    { return handle != other.handle; }

    int32_t handle = 0;
};

)cppcode";

//==============================================================================
static constexpr auto privateHelpers = R"cppcode(
//==============================================================================
//==============================================================================
//
//    All the code that follows this point should be considered internal.
//    User code should rarely need to refer to anything beyond this point..
//
//==============================================================================
//==============================================================================

template <typename Vec>  static Vec _vec_sqrt  (Vec a)         { return a.template apply<Vec> ([]  (typename Vec::ElementType x) { return SOUL_INTRINSICS::sqrt (x); }); }
template <typename Vec>  static Vec _vec_pow   (Vec a, Vec b)  { return a.template apply<Vec> ([&] (typename Vec::ElementType x) { return SOUL_INTRINSICS::pow (x, b); }); }
template <typename Vec>  static Vec _vec_exp   (Vec a)         { return a.template apply<Vec> ([]  (typename Vec::ElementType x) { return SOUL_INTRINSICS::exp (x); }); }
template <typename Vec>  static Vec _vec_log   (Vec a)         { return a.template apply<Vec> ([]  (typename Vec::ElementType x) { return SOUL_INTRINSICS::log (x); }); }
template <typename Vec>  static Vec _vec_log10 (Vec a)         { return a.template apply<Vec> ([]  (typename Vec::ElementType x) { return SOUL_INTRINSICS::log10 (x); }); }
template <typename Vec>  static Vec _vec_sin   (Vec a)         { return a.template apply<Vec> ([]  (typename Vec::ElementType x) { return SOUL_INTRINSICS::sin (x); }); }
template <typename Vec>  static Vec _vec_cos   (Vec a)         { return a.template apply<Vec> ([]  (typename Vec::ElementType x) { return SOUL_INTRINSICS::cos (x); }); }
template <typename Vec>  static Vec _vec_tan   (Vec a)         { return a.template apply<Vec> ([]  (typename Vec::ElementType x) { return SOUL_INTRINSICS::tan (x); }); }
template <typename Vec>  static Vec _vec_sinh  (Vec a)         { return a.template apply<Vec> ([]  (typename Vec::ElementType x) { return SOUL_INTRINSICS::sinh (x); }); }
template <typename Vec>  static Vec _vec_cosh  (Vec a)         { return a.template apply<Vec> ([]  (typename Vec::ElementType x) { return SOUL_INTRINSICS::cosh (x); }); }
template <typename Vec>  static Vec _vec_tanh  (Vec a)         { return a.template apply<Vec> ([]  (typename Vec::ElementType x) { return SOUL_INTRINSICS::tanh (x); }); }
template <typename Vec>  static Vec _vec_asinh (Vec a)         { return a.template apply<Vec> ([]  (typename Vec::ElementType x) { return SOUL_INTRINSICS::asinh (x); }); }
template <typename Vec>  static Vec _vec_acosh (Vec a)         { return a.template apply<Vec> ([]  (typename Vec::ElementType x) { return SOUL_INTRINSICS::acosh (x); }); }
template <typename Vec>  static Vec _vec_atanh (Vec a)         { return a.template apply<Vec> ([]  (typename Vec::ElementType x) { return SOUL_INTRINSICS::atanh (x); }); }
template <typename Vec>  static Vec _vec_asin  (Vec a)         { return a.template apply<Vec> ([]  (typename Vec::ElementType x) { return SOUL_INTRINSICS::asin (x); }); }
template <typename Vec>  static Vec _vec_acos  (Vec a)         { return a.template apply<Vec> ([]  (typename Vec::ElementType x) { return SOUL_INTRINSICS::acos (x); }); }
template <typename Vec>  static Vec _vec_atan  (Vec a)         { return a.template apply<Vec> ([]  (typename Vec::ElementType x) { return SOUL_INTRINSICS::atan (x); }); }
template <typename Vec>  static Vec _vec_atan2 (Vec a, Vec b)  { return a.template apply<Vec> ([&] (typename Vec::ElementType x) { return SOUL_INTRINSICS::atan2 (x, b); }); }

static constexpr int32_t _intrin_clamp (int32_t n, int32_t low, int32_t high)  { return n < low ? low : (n > high ? high : n); }
static constexpr int32_t _intrin_wrap (int32_t n, int32_t range)   { if (range == 0) return 0; auto x = n % range; return x < 0 ? x + range : x; }

static constexpr float  _nan32 = std::numeric_limits<float>::quiet_NaN();
static constexpr double _nan64 = std::numeric_limits<double>::quiet_NaN();

static constexpr float  _inf32 = std::numeric_limits<float>::infinity();
static constexpr double _inf64 = std::numeric_limits<double>::infinity();

static constexpr float  _ninf32 = -_inf32;
static constexpr double _ninf64 = -_inf64;

template <typename SourceFloatType, typename DestFloatType>
static inline void copyToInterleaved (DestFloatType* monoDest, const SourceFloatType* const* sourceChannels, uint32_t sourceStartFrame, uint32_t numFrames)
{
    auto source = *sourceChannels + sourceStartFrame;

    for (uint32_t i = 0; i < numFrames; ++i)
        monoDest[i] = static_cast<DestFloatType> (source[i]);
}

template <typename SourceFloatType, typename DestFloatType, int32_t numChannels>
static inline void copyToInterleaved (Vector<DestFloatType, numChannels>* vectorDest, const SourceFloatType* const* sourceChannels, uint32_t sourceStartFrame, uint32_t numFrames)
{
    for (uint32_t i = 0; i < numFrames; ++i)
        for (uint32_t chan = 0; chan < static_cast<uint32_t> (numChannels); ++chan)
            vectorDest[i].elements[chan] = static_cast<DestFloatType> (sourceChannels[chan][sourceStartFrame + i]);
}

template <typename SourceFloatType, typename DestFloatType>
static inline void copyFromInterleaved (DestFloatType* const* destChannels, uint32_t destStartFrame, const SourceFloatType* monoSource, uint32_t numFrames)
{
    auto dest = *destChannels + destStartFrame;

    for (uint32_t i = 0; i < numFrames; ++i)
        dest[i] = static_cast<DestFloatType> (monoSource[i]);
}

template <typename SourceFloatType, typename DestFloatType, int32_t numChannels>
static inline void copyFromInterleaved (DestFloatType* const* destChannels, uint32_t destStartFrame, const Vector<SourceFloatType, numChannels>* vectorSource, uint32_t numFrames)
{
    for (uint32_t i = 0; i < numFrames; ++i)
        for (uint32_t chan = 0; chan < static_cast<uint32_t> (numChannels); ++chan)
            destChannels[chan][destStartFrame + i] = static_cast<DestFloatType> (vectorSource[i].elements[chan]);
}
)cppcode";

//==============================================================================
static constexpr auto endpointStruct = R"cppcode(
using EndpointID = const char*;
enum class EndpointType     { value, stream, event };

struct EndpointDetails
{
    const char* name;
    EndpointID endpointID;
    EndpointType endpointType;
    const char* frameType;
    uint32_t numAudioChannels;
    const char* annotation;
};

)cppcode";

//==============================================================================
static constexpr auto pluginStructs = R"cppcode(
struct ParameterProperties
{
    const char* UID;
    const char* name;
    const char* unit;
    float minValue, maxValue, step, initialValue;
    bool isAutomatable, isBoolean, isHidden;
    const char* group;
    const char* textValues;
};

struct Parameter
{
    ParameterProperties properties;
    float currentValue;
    std::function<void(float)> applyValue;

    void setValue (float f)
    {
        currentValue = snapToLegalValue (f);
        applyValue (f);
    }

    float getValue() const
    {
        return currentValue;
    }

private:
    float snapToLegalValue (float v) const
    {
        if (properties.step > 0)
            v = properties.minValue + properties.step * SOUL_INTRINSICS::floor ((v - properties.minValue) / properties.step + 0.5f);

        return v < properties.minValue ? properties.minValue : (v > properties.maxValue ? properties.maxValue : v);
    }
};

struct AudioBus
{
    const char* name;
    uint32_t numChannels;
};
)cppcode";

static constexpr auto parameterList = R"cppcode(
struct ParameterList
{
    Parameter* begin()                      { return params; }
    Parameter* end()                        { return params + numParameters; }
    size_t size() const                     { return numParameters; }
    Parameter& operator[] (size_t index)    { SOUL_CPP_ASSERT (index < numParameters); return params[index]; }

    Parameter params[numParameters == 0 ? 1 : numParameters];
};
)cppcode";

//==============================================================================
static constexpr auto juceHeaderClass = R"cppcode(
struct CLASS_NAME   : public juce::AudioPluginInstance
{
    CLASS_NAME();
    ~CLASS_NAME() override;

    //==============================================================================
    void fillInPluginDescription (juce::PluginDescription&) const override;
    void refreshParameterList() override;

    //==============================================================================
    const juce::String getName() const override;
    juce::StringArray getAlternateDisplayNames() const override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;

    //==============================================================================
    void reset() override;
    void prepareToPlay (double sampleRate, int maxBlockSize) override;
    void releaseResources() override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    bool hasEditor() const override;
    juce::AudioProcessorEditor* createEditor() override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int size) override;

    //==============================================================================
    double getTailLengthSeconds() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool supportsMPE() const override;
    bool isMidiEffect() const override;
    void setNonRealtime (bool) noexcept override;

protected:
    //==============================================================================
    struct EditorSize  { int width = 0, height = 0; };
    EditorSize getStoredEditorSize (EditorSize defaultSize);
    void storeEditorSize (EditorSize newSize);

    juce::MidiKeyboardState midiKeyboardState;

    //==============================================================================
private:
    struct Pimpl;
    struct Parameter;
    struct EditorComponent;

    std::unique_ptr<Pimpl> pimpl;
    std::vector<Parameter*> allParameters;
    std::vector<std::unique_ptr<Parameter>> hiddenParams;
    juce::ValueTree lastValidState;

    juce::ValueTree createCurrentState();
    void updateLastState();
    void ensureValidStateExists();
    void applyLastState();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CLASS_NAME)
};
)cppcode";


static constexpr auto juceCPP = R"cppcode(
//==============================================================================
//==============================================================================
//
// The rest of this file is now the implementation of juce::AudioPluginInstance
//
//==============================================================================
//==============================================================================

struct CLASS_NAME::Pimpl
{
    Pimpl (CLASS_NAME& p) : owner (p) {}

    using GeneratedClass = GENERATED_CLASS;

    CLASS_NAME& owner;
    GeneratedClass processor;
    int sessionID = 0;
    std::atomic<uint32_t> numParametersNeedingUpdate { 0 };

    juce::AudioBuffer<float> outputBuffer;
    std::vector<GeneratedClass::MIDIMessage> incomingMIDIMessages;

    struct IDs
    {
        const juce::Identifier UID      { GeneratedClass::UID },
                               id       { "id" },
                               version  { "version" },
                               value    { "value" },
                               size     { "size" },
                               PARAM    { "PARAM" },
                               EDITOR   { "EDITOR" };
    };

    IDs ids;

    static juce::AudioProcessor::BusesProperties createBuses()
    {
        juce::AudioProcessor::BusesProperties buses;

        if constexpr (GeneratedClass::numAudioInputChannels > 0)
            buses.addBus (true, "Input", juce::AudioChannelSet::canonicalChannelSet (GeneratedClass::numAudioInputChannels));

        if constexpr (GeneratedClass::numAudioOutputChannels > 0)
            buses.addBus (false, "Output", juce::AudioChannelSet::canonicalChannelSet (GeneratedClass::numAudioOutputChannels));

        return buses;
    }

    void prepareToPlay (double sampleRate, int maxBlockSize)
    {
        processor.init (sampleRate, ++sessionID);

        incomingMIDIMessages.resize ((size_t) maxBlockSize);
        owner.setRateAndBufferSizeDetails (sampleRate, maxBlockSize);
        owner.midiKeyboardState.reset();
        outputBuffer.setSize (GeneratedClass::numAudioOutputChannels, maxBlockSize, false, false, true);
        updateAllParameters();
    }

    template <class RenderContext>
    static void populateInputChannels (juce::AudioBuffer<float>& audio, RenderContext& rc)
    {
        if constexpr (GeneratedClass::numAudioInputChannels > 0)
        {
            for (int i = 0; i < static_cast<int> (GeneratedClass::numAudioInputChannels); ++i)
                rc.inputChannels[static_cast<size_t> (i)] = audio.getReadPointer (i);
        }
    }

    void processBlock (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
    {
        auto numFrames = audio.getNumSamples();
        outputBuffer.setSize (GeneratedClass::numAudioOutputChannels, numFrames, false, false, true);
        outputBuffer.clear();

        GeneratedClass::RenderContext<float> rc;

        populateInputChannels (audio, rc);

        for (int i = 0; i < (int) GeneratedClass::numAudioOutputChannels; ++i)
            rc.outputChannels[static_cast<size_t> (i)] = outputBuffer.getWritePointer (i);

        rc.numFrames = (uint32_t) numFrames;

        owner.midiKeyboardState.processNextMidiBuffer (midi, 0, numFrames, true);

        if (midi.isEmpty())
        {
            rc.incomingMIDI.numMessages = 0;
        }
        else
        {
            auto maxEvents = incomingMIDIMessages.size();
            auto iter = midi.cbegin();
            auto end = midi.cend();
            size_t i = 0;

            while (i < maxEvents && iter != end)
            {
                auto message = *iter++;

                if (message.numBytes < 4)
                    incomingMIDIMessages[i++] = { static_cast<uint32_t> (message.samplePosition),
                                                  static_cast<uint8_t> (message.data[0]),
                                                  static_cast<uint8_t> (message.data[1]),
                                                  static_cast<uint8_t> (message.data[2]) };
            }

            rc.incomingMIDI.messages = std::addressof (incomingMIDIMessages[0]);
            rc.incomingMIDI.numMessages = (uint32_t) i;
            midi.clear();
        }

        if constexpr (GeneratedClass::hasTimelineEndpoints)
            if (auto playhead = owner.getPlayHead())
                updatePlayheadState (*playhead);

        updateAnyChangedParameters();

        processor.render (rc);

        for (int i = 0; i < outputBuffer.getNumChannels(); ++i)
            audio.copyFrom (i, 0, outputBuffer, i, 0, numFrames);
    }

    void reset();
    void updateAllParameters();
    void updateAnyChangedParameters();

    enum class TransportState
    {
        stopped    = 0,
        playing    = 1,
        recording  = 2
    };

    void updatePlayheadState (juce::AudioPlayHead& playhead)
    {
        juce::AudioPlayHead::CurrentPositionInfo info;

        if (playhead.getCurrentPosition (info))
        {
            processor.setTimeSignature (static_cast<int32_t> (info.timeSigNumerator),
                                        static_cast<int32_t> (info.timeSigDenominator));

            processor.setTempo (static_cast<float> (info.bpm));

            processor.setPosition (static_cast<int64_t> (info.timeInSamples),
                                   static_cast<double> (info.ppqPosition),
                                   static_cast<double> (info.ppqPositionOfLastBarStart));

            processor.setTransportState (static_cast<int32_t> (info.isRecording ? TransportState::recording
                                                                                : (info.isPlaying ? TransportState::playing
                                                                                                  : TransportState::stopped)));
        }
    }
};
)cppcode" // NB: this long string literal is broken up so that MSVC doesn't choke on it
R"cppcode(
//==============================================================================
CLASS_NAME::CLASS_NAME()
   : juce::AudioPluginInstance (Pimpl::createBuses())
{
    pimpl = std::make_unique<Pimpl> (*this);
    setLatencySamples (static_cast<int> (Pimpl::GeneratedClass::latency));
    refreshParameterList();
}

CLASS_NAME::~CLASS_NAME() = default;

//==============================================================================
void CLASS_NAME::fillInPluginDescription (juce::PluginDescription& d) const
{
    d.name                = Pimpl::GeneratedClass::name;
    d.descriptiveName     = Pimpl::GeneratedClass::description;
    d.pluginFormatName    = "Compiled SOUL Patch";
    d.category            = Pimpl::GeneratedClass::category;
    d.manufacturerName    = Pimpl::GeneratedClass::manufacturer;
    d.version             = Pimpl::GeneratedClass::version;
    d.fileOrIdentifier    = {};
    d.lastFileModTime     = {};
    d.lastInfoUpdateTime  = {};
    d.uid                 = (int) juce::String (Pimpl::GeneratedClass::UID).hash();
    d.isInstrument        = Pimpl::GeneratedClass::isInstrument;
}

const juce::String CLASS_NAME::getName() const    { return Pimpl::GeneratedClass::name; }

juce::StringArray CLASS_NAME::getAlternateDisplayNames() const
{
    juce::StringArray s;
    s.add (Pimpl::GeneratedClass::name);

    if (Pimpl::GeneratedClass::description[0] != 0)
        s.add (Pimpl::GeneratedClass::description);

    return s;
}

bool CLASS_NAME::isBusesLayoutSupported (const BusesLayout& layout) const
{
    auto processorInputBuses  = pimpl->processor.getInputBuses();
    auto processorOutputBuses = pimpl->processor.getOutputBuses();

    if (layout.inputBuses.size() != (int) processorInputBuses.size())
        return false;

    if (layout.outputBuses.size() != (int) processorOutputBuses.size())
        return false;

    for (size_t i = 0; i < processorInputBuses.size(); ++i)
        if ((int) processorInputBuses[i].numChannels != layout.getNumChannels (true, (int) i))
            return false;

    for (size_t i = 0; i < processorOutputBuses.size(); ++i)
        if ((int) processorOutputBuses[i].numChannels != layout.getNumChannels (false, (int) i))
            return false;

    return true;
}

//==============================================================================
void CLASS_NAME::reset()                                                { pimpl->reset(); }
void CLASS_NAME::prepareToPlay (double sampleRate, int maxBlockSize)    { pimpl->prepareToPlay (sampleRate, maxBlockSize); }
void CLASS_NAME::releaseResources()                                     { midiKeyboardState.reset(); }

void CLASS_NAME::processBlock (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    if (isSuspended())
    {
        for (int i = 0; i < (int) Pimpl::GeneratedClass::numAudioOutputChannels; ++i)
            audio.clear (i, 0, audio.getNumSamples());
    }
    else
    {
        pimpl->processBlock (audio, midi);
    }
}

//==============================================================================
int CLASS_NAME::getNumPrograms()                               { return 1; }
int CLASS_NAME::getCurrentProgram()                            { return 0; }
void CLASS_NAME::setCurrentProgram (int)                       {}
const juce::String CLASS_NAME::getProgramName (int)            { return {}; }
void CLASS_NAME::changeProgramName (int, const juce::String&)  {}

double CLASS_NAME::getTailLengthSeconds() const                { return 0; }
bool CLASS_NAME::acceptsMidi() const                           { return Pimpl::GeneratedClass::hasMIDIInput; }
bool CLASS_NAME::producesMidi() const                          { return false; }
bool CLASS_NAME::supportsMPE() const                           { return Pimpl::GeneratedClass::hasMIDIInput; }
bool CLASS_NAME::isMidiEffect() const                          { return acceptsMidi() && producesMidi(); }
void CLASS_NAME::setNonRealtime (bool) noexcept                {}
)cppcode" // NB: this long string literal is broken up so that MSVC doesn't choke on it
R"cppcode(
//==============================================================================
struct CLASS_NAME::Parameter  : public juce::AudioProcessorParameterWithID
{
    Parameter (CLASS_NAME& owner, const Pimpl::GeneratedClass::Parameter& p)
        : AudioProcessorParameterWithID (p.properties.UID, p.properties.name),
          param (p),
          textValues (parseTextValues (param.properties.textValues)),
          range (param.properties.minValue, param.properties.maxValue, param.properties.step),
          numDecimalPlaces (getNumDecimalPlaces (range)),
          numParametersNeedingUpdate (owner.pimpl->numParametersNeedingUpdate)
    {
        currentFullRangeValue = param.currentValue;
    }

    float currentFullRangeValue = 0;
    bool needsUpdate = false;

    Pimpl::GeneratedClass::Parameter param;
    const juce::StringArray textValues;
    const juce::NormalisableRange<float> range;
    const int numDecimalPlaces;
    std::atomic<uint32_t>& numParametersNeedingUpdate;

    juce::String getName (int maximumStringLength) const override    { return name.substring (0, maximumStringLength); }
    juce::String getLabel() const override                           { return param.properties.unit; }
    Category getCategory() const override                            { return genericParameter; }
    bool isDiscrete() const override                                 { return range.interval != 0; }
    bool isBoolean() const override                                  { return param.properties.isBoolean; }
    bool isAutomatable() const override                              { return param.properties.isAutomatable; }
    bool isMetaParameter() const override                            { return false; }
    juce::StringArray getAllValueStrings() const override            { return textValues; }
    float getDefaultValue() const override                           { return convertTo0to1 (param.properties.initialValue); }
    float getValue() const override                                  { return convertTo0to1 (currentFullRangeValue); }
    void setValue (float newValue) override                          { setFullRangeValue (convertFrom0to1 (newValue)); }

    void setFullRangeValue (float newValue)
    {
        if (newValue != currentFullRangeValue)
        {
            currentFullRangeValue = newValue;
            markAsDirty();
            sendValueChangedMessageToListeners (convertTo0to1 (newValue));
        }
    }

    void markAsDirty()
    {
        if (! needsUpdate)
        {
            needsUpdate = true;
            ++numParametersNeedingUpdate;
        }
    }

    void sendUpdate()
    {
        param.setValue (currentFullRangeValue);
    }

    bool sendUpdateIfNeeded()
    {
        if (! needsUpdate)
            return false;

        needsUpdate = false;
        sendUpdate();
        return true;
    }

    juce::String getText (float v, int length) const override
    {
        if (length > 0)              return getText (v, 0).substring (0, length);
        if (textValues.isEmpty())    return juce::String (convertFrom0to1 (v), numDecimalPlaces);
        if (textValues.size() == 1)  return preprocessText (textValues[0].toUTF8(), convertFrom0to1 (v));

        return textValues[juce::jlimit (0, textValues.size() - 1, juce::roundToInt (v * (textValues.size() - 1.0f)))];
    }

    float getValueForText (const juce::String& text) const override
    {
        for (int i = 0; i < textValues.size(); ++i)
            if (textValues[i] == text)
                return i / (textValues.size() - 1.0f);

        return convertTo0to1 (text.upToLastOccurrenceOf (text, false, false).getFloatValue());
    }

    int getNumSteps() const override
    {
        if (! textValues.isEmpty() && std::abs (textValues.size() - (range.end - range.start)) < 0.01f)
            return textValues.size() - 1;

        if (range.interval > 0)
            return static_cast<int> ((range.end - range.start) / range.interval) + 1;

        return AudioProcessor::getDefaultNumParameterSteps();
    }

private:
    float convertTo0to1 (float v) const    { return range.convertTo0to1 (range.snapToLegalValue (v)); }
    float convertFrom0to1 (float v) const  { return range.snapToLegalValue (range.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, v))); }

    static int getNumDecimalPlaces (juce::NormalisableRange<float> r)
    {
        int places = 7;

        if (r.interval != 0.0f)
        {
            if (juce::approximatelyEqual (std::abs (r.interval - (int) r.interval), 0.0f))
                return 0;

            auto v = std::abs (juce::roundToInt (r.interval * pow (10, places)));

            while ((v % 10) == 0 && places > 0)
            {
                --places;
                v /= 10;
            }
        }

        return places;
    }

    static juce::StringArray parseTextValues (const juce::String& text)
    {
        if (text.isNotEmpty())
            return juce::StringArray::fromTokens (text.unquoted(), "|", {});

        return {};
    }

    static juce::String preprocessText (juce::CharPointer_UTF8 text, float value)
    {
        juce::MemoryOutputStream result;

        while (! text.isEmpty())
        {
            auto c = text.getAndAdvance();

            if (c != '%')  { result << juce::String::charToString (c); continue; }

            auto format = text;
            bool addSignChar = (*format == '+');
            if (addSignChar) ++format;

            bool isPadded = (*format == '0');
            int numDigits = 0;

            while (format.isDigit())
                numDigits = numDigits * 10 + (format.getAndAdvance() - '0');

            bool isFloat = (*format == 'f');
            bool isInt   = (*format == 'd');

            if (! (isInt || isFloat))
            {
                result << '%';
                continue;
            }

            if (addSignChar && value >= 0)
                result << '+';

            if (isInt)
            {
                juce::String s ((int64_t) (value + 0.5f));
                result << (isPadded ? s.paddedLeft ('0', numDigits) : s);
            }
            else if (numDigits <= 0)
            {
                result << value;
            }
            else if (isPadded)
            {
                result << juce::String (value, numDigits);
            }
            else
            {
                juce::String s (value);
                auto afterDot = s.fromLastOccurrenceOf (".", false, false);

                if (afterDot.containsOnly ("0123456789"))
                    if (afterDot.length() > numDigits)
                        s = s.dropLastCharacters (afterDot.length() - numDigits);

                result << s;
            }

            text = ++format;
        }

        return result.toString();
    }
};
)cppcode" // NB: this long string literal is broken up so that MSVC doesn't choke on it
R"cppcode(
void CLASS_NAME::Pimpl::reset()
{
    processor.reset();

    for (auto& p : owner.allParameters)
        p->markAsDirty();
}

void CLASS_NAME::Pimpl::updateAllParameters()
{
    for (auto& p : owner.allParameters)
        p->sendUpdate();
}

void CLASS_NAME::Pimpl::updateAnyChangedParameters()
{
    if (numParametersNeedingUpdate != 0)
        for (auto& p : owner.allParameters)
            if (p->sendUpdateIfNeeded())
                if (--numParametersNeedingUpdate == 0)
                    break;
}

void CLASS_NAME::refreshParameterList()
{
    struct ParameterTreeGroupBuilder
    {
        std::map<juce::String, juce::AudioProcessorParameterGroup*> groups;
        juce::AudioProcessorParameterGroup tree;

        void addParam (std::unique_ptr<Parameter> newParam)
        {
            juce::String group (newParam->param.properties.group);

            if (group.isNotEmpty())
                getOrCreateGroup (tree, {}, group).addChild (std::move (newParam));
            else
                tree.addChild (std::move (newParam));
        }

        juce::AudioProcessorParameterGroup& getOrCreateGroup (juce::AudioProcessorParameterGroup& targetTree,
                                                              const juce::String& parentPath,
                                                              const juce::String& subPath)
        {
            auto fullPath = parentPath + "/" + subPath;
            auto& targetGroup = groups[fullPath];

            if (targetGroup != nullptr)
                return *targetGroup;

            auto slash = subPath.indexOfChar ('/');

            if (slash < 0)
            {
                auto newGroup = std::make_unique<juce::AudioProcessorParameterGroup> (fullPath, subPath, "/");
                targetGroup = newGroup.get();
                targetTree.addChild (std::move (newGroup));
                return *targetGroup;
            }

            auto firstPathPart = subPath.substring (0, slash);
            auto& parentGroup = getOrCreateGroup (targetTree, parentPath, firstPathPart);
            return getOrCreateGroup (parentGroup, parentPath + "/" + firstPathPart, subPath.substring (slash + 1));
        }
    };

    ParameterTreeGroupBuilder treeBuilder;

    for (auto& p : pimpl->processor.createParameterList())
    {
        auto param = std::make_unique<Parameter> (*this, p);
        allParameters.push_back (param.get());

        if (p.properties.isHidden)
            hiddenParams.push_back (std::move (param));
        else
            treeBuilder.addParam (std::move (param));
    }

    setParameterTree (std::move (treeBuilder.tree));
    pimpl->numParametersNeedingUpdate = static_cast<uint32_t> (allParameters.size());
}
)cppcode" // NB: this long string literal is broken up so that MSVC doesn't choke on it
R"cppcode(
//==============================================================================
void CLASS_NAME::getStateInformation (juce::MemoryBlock& data)
{
    updateLastState();
    juce::MemoryOutputStream out (data, false);
    lastValidState.writeToStream (out);
}

void CLASS_NAME::setStateInformation (const void* data, int size)
{
    auto newState = juce::ValueTree::readFromData (data, (size_t) size);

    if (newState.hasType (pimpl->ids.UID))
    {
        lastValidState = std::move (newState);
        applyLastState();
    }
}

juce::ValueTree CLASS_NAME::createCurrentState()
{
    juce::ValueTree state (pimpl->ids.UID);
    state.setProperty (pimpl->ids.version, Pimpl::GeneratedClass::version, nullptr);

    auto editorState = lastValidState.getChildWithName (pimpl->ids.EDITOR);

    if (editorState.isValid())
        state.addChild (editorState.createCopy(), -1, nullptr);

    for (auto& p : allParameters)
    {
        juce::ValueTree param (pimpl->ids.PARAM);
        param.setProperty (pimpl->ids.id, p->param.properties.UID, nullptr);
        param.setProperty (pimpl->ids.value, p->currentFullRangeValue, nullptr);
        state.addChild (param, -1, nullptr);
    }

    return state;
}

void CLASS_NAME::updateLastState()
{
    lastValidState = createCurrentState();
}

void CLASS_NAME::ensureValidStateExists()
{
    if (! lastValidState.hasType (pimpl->ids.UID))
        updateLastState();
}

void CLASS_NAME::applyLastState()
{
    if (lastValidState.hasType (pimpl->ids.UID))
        for (auto& p : allParameters)
            if (auto* value = lastValidState.getChildWithProperty (pimpl->ids.id, p->param.properties.UID).getPropertyPointer (pimpl->ids.value))
                p->setFullRangeValue (*value);
}

//==============================================================================
struct CLASS_NAME::EditorComponent  : public juce::AudioProcessorEditor
{
    EditorComponent (CLASS_NAME& p)
        : juce::AudioProcessorEditor (p), owner (p), editor (p),
          midiKeyboard (p.midiKeyboardState, juce::MidiKeyboardComponent::Orientation::horizontalKeyboard)
    {
        setLookAndFeel (&lookAndFeel);

        if constexpr (Pimpl::GeneratedClass::numParameters != 0)
            addAndMakeVisible (editor);

        if (Pimpl::GeneratedClass::hasMIDIInput)
            addAndMakeVisible (midiKeyboard);

        auto size = owner.getStoredEditorSize ({ 600, 400 });
        setResizeLimits (400, 150, 2000, 2000);
        setSize (size.width, size.height);
    }

    ~EditorComponent() override
    {
        owner.editorBeingDeleted (this);
        setLookAndFeel (nullptr);
    }

    void paint (juce::Graphics& g) override
    {
        auto background = lookAndFeel.findColour (juce::ResizableWindow::backgroundColourId);
        g.fillAll (background);

        if (getNumChildComponents() == 0)
        {
            g.setColour (background.contrasting());
            g.setFont (16.0f);
            g.drawFittedText (owner.getName(), getLocalBounds().reduced (6), juce::Justification::centred, 2);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (6);

        if (midiKeyboard.isVisible())
            midiKeyboard.setBounds (r.removeFromBottom (std::min (80, r.getHeight() / 4)));

        editor.setBounds (r);
        owner.storeEditorSize ({ getWidth(), getHeight() });
    }

    CLASS_NAME& owner;
    juce::LookAndFeel_V4 lookAndFeel;
    juce::GenericAudioProcessorEditor editor;
    juce::MidiKeyboardComponent midiKeyboard;
};
)cppcode" // NB: this long string literal is broken up so that MSVC doesn't choke on it
R"cppcode(
bool CLASS_NAME::hasEditor() const                         { return true; }
juce::AudioProcessorEditor* CLASS_NAME::createEditor()     { return new EditorComponent (*this); }

CLASS_NAME::EditorSize CLASS_NAME::getStoredEditorSize (EditorSize defaultSize)
{
    auto propertyValue = lastValidState.getChildWithName (pimpl->ids.EDITOR).getProperty (pimpl->ids.size);
    auto tokens = juce::StringArray::fromTokens (propertyValue.toString(), " ", {});

    if (tokens.size() == 2)
    {
        auto w = tokens[0].getIntValue();
        auto h = tokens[1].getIntValue();

        if (w > 0 && h > 0)
            return { w, h };
    }

    return defaultSize;
}

void CLASS_NAME::storeEditorSize (EditorSize newSize)
{
    ensureValidStateExists();
    auto state = lastValidState.getOrCreateChildWithName (pimpl->ids.EDITOR, nullptr);

    if (newSize.width > 0 || newSize.height > 0)
        state.setProperty (pimpl->ids.size, juce::String (newSize.width) + " " + juce::String (newSize.height), nullptr);
    else
        state.removeProperty (pimpl->ids.size, nullptr);
}
)cppcode";

}
