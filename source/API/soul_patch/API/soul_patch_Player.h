/*
     _____ _____ _____ __
    |   __|     |  |  |  |
    |__   |  |  |  |  |  |__
    |_____|_____|_____|_____|

    Copyright (c) 2018 - ROLI Ltd.
*/

#if ! SOUL_PATCH_MAIN_INCLUDE_FILE
 #error "This header must not be included directly in your code - include soul_patch.h instead"
#endif

namespace soul
{
namespace patch
{

//==============================================================================
/** The set of properties that are known about a patch before it is compiled.
    Most of these are taken directly from the .soulpatch manifest file contents.
*/
struct Description
{
    /** Provides acces to the .soulpatch manifest file from which this patch was loaded. */
    VirtualFile::Ptr manifestFile;

    String::Ptr UID, version;
    String::Ptr name, description, category, manufacturer, URL;
    bool isInstrument = false;
};

//==============================================================================
/** A time-stamped short MIDI message. */
struct MIDIMessage
{
    /** The frame index is a sample offset into the current block of data being
        processed by a call to PatchPlayer::render().
    */
    uint32_t frameIndex;

    uint8_t byte0, byte1, byte2;
};

//==============================================================================
/** Gives information about one of the patch's buses.
    Currently this is a minimal bus description, just providing the number of
    channels. In the longer term, this will be expanded to include details such
    as channel layouts and speaker assignments.
*/
struct Bus
{
    String::Ptr name;
    uint32_t numChannels = 0;
};

//==============================================================================
/** Provides access to a parameter's value and properties. */
struct Parameter  : public RefCountedBase
{
    using Ptr = RefCountingPtr<Parameter>;

    String::Ptr ID, name, unit;
    float minValue = 0, maxValue = 1.0f, step = 0, initialValue = 0;

    /** Returns the current value of this parameter. */
    virtual float getValue() const = 0;

    /** Changes the value of this parameter.
        The value that is passed-in will be clamped to the valid range, and
        if a step vaiue is specified, it will also be quantised.
    */
    virtual void setValue (float newValue) = 0;

    /** Returns one of the properties from the annotation on the SOUL stream.
        If there's no property with this name, it will return a nullptr.
    */
    virtual String::Ptr getProperty (const char* propertyName) const = 0;

    /** Returns the names of all the annotations on the SOUL stream. */
    virtual Span<const char*> getPropertyNames() const = 0;
};

//==============================================================================
/** Holds the settings needed when compiling an instance of a PatchPlayer. */
struct PatchPlayerConfiguration
{
    double sampleRate = 0;
    uint32_t maxFramesPerBlock = 0;
};

//==============================================================================
/** Description of an error or warning message. */
struct CompilationMessage
{
    String::Ptr fullMessage, filename, description;
    uint32_t line = 0, column = 0;
    bool isError = false;
};

//==============================================================================
/**
    A PatchPlayer is created by calling PatchInstance::compileNewPlayer().

    Once created, a PatchPlayer provides more detailed information about the
    parameters and buses, and can actually render audio. While running, the
    only modifications that can be made are to parameters - if anything else
    changes, such as the sample rate, the block size, or the soul patch source
    code, then a new PatchPlayer must be created to replace the old one.
*/
class PatchPlayer  : public RefCountedBase
{
public:
    using Ptr = RefCountingPtr<PatchPlayer>;

    /** If compilation failed, this will return one or more error messages and
        the player can't be used.
        If the player compiled successfully, this will return either an empty list,
        or a list of just warnings. You should always be careful to check this before
        using the player object!
        @see isPlayable
    */
    virtual Span<CompilationMessage> getCompileMessages() const = 0;

    /** Returns true if the compilation succeeded (possibly with warnings) and the
        player can be run.
        @see getCompileMessages
    */
    virtual bool isPlayable() const = 0;

    /** Returns a Description object containing all the details about this patch. */
    virtual Description getDescription() const = 0;

    /** Checks whether the configuration or other internal factors (such as the source
        files of the patch) have changed in a way that means this player is out of date
        and should be replaced by a newly compiled player instance.
    */
    virtual bool needsRebuilding (const PatchPlayerConfiguration&) = 0;

    //==============================================================================
    /** Returns a list of the input buses that this patch provides. */
    virtual Span<Bus> getInputBuses() const = 0;

    /** Returns a list of the output buses that this patch provides. */
    virtual Span<Bus> getOutputBuses() const = 0;

    /** Returns a list of patch's parameters. */
    virtual Span<Parameter::Ptr> getParameters() const = 0;

    //==============================================================================
    /** This will reset the state of the player to its initial state.
        Calls to this method must not be made concurrently with the render() method!
    */
    virtual void reset() = 0;

    /** Return value for the PatchPlayer::render() method. */
    enum class RenderResult
    {
        ok,
        noProgramLoaded,
        wrongNumberOfChannels
    };

    /** Contains the info needed for a call to the PatchPlayer::render() method. */
    struct RenderContext
    {
        /** A set of pointers to input channel data for the render() method to read.
            See the numInputChannels variable for the number of channels, and numFrames for
            the number of samples in each channel.
            None of the pointers in this array may be null.
        */
        const float* const* inputChannels;

        /** A set of pointers to output channel data for the render() method to write.
            See the numOutputChannels variable for the number of channels, and numFrames for
            the number of samples in each channel.
            None of the pointers in this array may be null.
        */
        float* const* outputChannels;

        /** An array of MIDI messages for the render method to process.
            See the numMIDIMessages variable for the number of channels.
        */
        const MIDIMessage* incomingMIDI;

        /** An array of MIDI messages for the render method to write to.
            See the numMIDIMessages variable for the number of channels.
        */
        MIDIMessage* outgoingMIDI;

        /** Number of audio frames to process */
        uint32_t numFrames;

        /** Number of channels in the input stream array */
        uint32_t numInputChannels;

        /** Number of channels in the output stream array */
        uint32_t numOutputChannels;

        /** Number of messages to process from the incomingMIDI buffer */
        uint32_t numMIDIMessagesIn;

        /** The maximum number of messages that can be added to the outgoingMIDI buffer */
        uint32_t maximumMIDIMessagesOut;

        /** On return, this will be set to the number of MIDI messages that could have been
            added to the outgoingMIDI buffer. If more messages were available than the
            maximumMIDIMessagesOut size, then the buffer will have been filled up to
            maximumMIDIMessagesOut, and numMIDIMessagesOut will return a larger number to
            indicate how many would have been added if it had been possible. */
        uint32_t numMIDIMessagesOut;
    };

    /** Renders the next block of audio.
        The RenderContext object provides all the necessary input/output audio and
        MIDI data that is needed.
    */
    virtual RenderResult render (RenderContext&) = 0;
};

} // namespace patch
} // namespace soul
