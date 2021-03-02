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

//==============================================================================
struct CPPGenerator
{
    CPPGenerator (choc::text::CodePrinter& cp, Program p, CodeGenOptions& o)
        : program (std::move (p)), options (o), mainProcessor (program.getMainProcessor()), stream (cp)
    {
    }

    static constexpr choc::text::CodePrinter::NewLine newLine = {};
    static constexpr choc::text::CodePrinter::BlankLine blankLine = {};
    static constexpr choc::text::CodePrinter::SectionBreak sectionBreak = {};

    bool run()
    {
        stream.setLineWrapLength (300);
        stream << choc::text::trimStart (choc::text::replace (headerComment, "VERSION",  getLibraryVersion().toString()))
               << blankLine;

        if (options.generateJUCECPP)
        {
            stream << choc::text::trimStart (juceHeaderGuard)
                   << blankLine
                   << choc::text::replace (choc::text::trimStart (matchingHeaderGuard), "HEADER_HASH", getJUCEHeaderHashSymbol());
        }
        else if (options.generateJUCEHeader)
        {
            stream << "#pragma once" << blankLine;
            stream << choc::text::trimStart (juceHeaderGuard);
        }
        else
        {
            stream << choc::text::trimStart (standardIncludes);
        }

        stream << blankLine
               << choc::text::trimStart (definitions)
               << blankLine;

        if (! options.classNamespace.empty())
            stream << "namespace " << options.classNamespace << newLine
                   << "{" << blankLine;

        if (options.className.empty())
            options.className = mangleStructOrFunctionName (mainProcessor.getReadableName());

        if (options.className != makeSafeIdentifier (options.className))
            CodeLocation().throwError (Errors::invalidName (options.className));

        if (options.generateJUCEHeader)
            printJUCEHeader (options.className);
        else if (options.generateJUCECPP)
            printJUCECpp (options.className);
        else
            printMainClass (options.className);

        if (! options.classNamespace.empty())
            stream << "}  // namespace " << options.classNamespace << newLine;

        return true;
    }

    //==============================================================================
    const Program program;
    CodeGenOptions& options;
    Module& mainProcessor;

    choc::text::CodePrinter& stream;

    std::unordered_map<pool_ref<const heart::Variable>, std::string> localVariableNames;
    const soul::Module* currentModule = nullptr;

    struct ExternalDataFunction
    {
        ConstantTable::Handle handle;
        Type type;
        std::string name, value;
    };

    std::vector<ExternalDataFunction> externalDataFunctions;

    uint32_t getMaxBlockSize() const   { return options.buildSettings.maxBlockSize; }

    //==============================================================================
    void printMainClass (const std::string& className)
    {
        if (options.packStructures)
            stream << "#pragma pack (push, 1)" << blankLine;

        printSourceDescriptionComment();

        stream << "class " << className << newLine
               << "{" << newLine
               << "public:" << newLine;

        {
            auto indent = stream.createIndent();
            printConstructorAndDestructor (className);
            stream << sectionBreak
                   << choc::text::replace (choc::text::trimStart (forwardDecls),
                                           "MAX_BLOCK_SIZE", std::to_string (getMaxBlockSize()),
                                           "LATENCY", std::to_string (mainProcessor.latency));

            printStaticConstants();
            printStructs (true);
            stream << "struct StringLiteral;" << newLine;
            printEssentialMethods();

            if (options.generateRenderMethod || options.generatePluginMethods)
                printIntegratedRenderMethod();

            printDirectPerformerMethods();

            if (options.createEndpointFunctions)
                printEndpointListMethods();

            if (options.generatePluginMethods)
                printPluginMethods();

            stream << sectionBreak
                   << choc::text::trimStart (helperClasses);

            printPrivateContent();
        }

        stream << "};" << blankLine;

        if (options.packStructures)
            stream << "#pragma pack (pop)" << newLine;
    }

    void printJUCEHeader (const std::string& className)
    {
        printSourceDescriptionComment();
        stream << choc::text::replace (choc::text::trimStart (juceHeaderClass), "CLASS_NAME", className)
               << blankLine
               << "#define " << getJUCEHeaderHashSymbol() << " 1" << blankLine;
    }

    void printJUCECpp (const std::string& className)
    {
        auto internalClassName = "SOUL_" + className;
        printMainClass (internalClassName);

        stream << choc::text::replace (choc::text::trimStart (juceCPP),
                                       "CLASS_NAME", className,
                                       "GENERATED_CLASS", internalClassName)
               << blankLine;
    }

    std::string getJUCEHeaderHashSymbol() const
    {
        uint64_t hash = 0;

        for (auto c : program.getHash())
            hash = hash * 65537u + (uint8_t) c;

        return "SOUL_HEADER_INCLUDED_" + std::to_string (hash).substr (0, 9);
    }

    //==============================================================================
    void printSourceDescriptionComment()
    {
        stream << sectionBreak
               << "// Generated from " << (mainProcessor.isGraph() ? "graph" : "processor")
               << " " << choc::text::addSingleQuotes (mainProcessor.getReadableName());

        if (! options.sourceDescription.empty())
            stream << ", " << options.sourceDescription;

        stream << newLine
               << "//" << newLine;
    }

    void printConstructorAndDestructor (const std::string& className)
    {
        stream << className << "() = default;" << newLine
               << "~" << className << "() = default;" << newLine;
    }

    void printEssentialMethods()
    {
        stream << sectionBreak
               << "// The following methods provide basic initialisation and control for the processor" << newLine
               << essentialMethods;

        printGetXRuns();
    }

    void printGetXRuns()
    {
        stream << "uint32_t getNumXRuns() noexcept" << newLine;
        auto indent = stream.createIndentWithBraces();
        stream << "return static_cast<uint32_t> (" << FunctionNames::getNumXRunsFunctionName() << " (state));" << newLine;
    }

    //==============================================================================
    void printIntegratedRenderMethod()
    {
        stream << sectionBreak
               << "// These classes and functions provide a high-level rendering method that" << newLine
               << "// presents the processor as a set of standard audio and MIDI channels." << newLine;

        auto midiIns = findMIDIInputs();
        auto audioIns = findAudioInputs();
        auto audioOuts = findAudioOutputs();

        auto numInputChans  = getTotalAudioChannels (audioIns);
        auto numOutputChans = getTotalAudioChannels (audioOuts);

        stream << choc::text::replace (renderHelperClasses,
                                       "NUM_AUDIO_OUT_CHANS", std::to_string (numOutputChans),
                                       "NUM_AUDIO_IN_CHANS", std::to_string (numInputChans))
               << sectionBreak
               << "template <typename FloatType>" << newLine
               << "void render (RenderContext<FloatType> context)" << newLine;

        {
            auto indent1 = stream.createIndentWithBraces();

            stream << "uint32_t startFrame = 0";

            if (! midiIns.empty())
                stream << ", startMIDIIndex = 0";

            stream << ";" << blankLine
                   << "while (startFrame < context.numFrames)" << newLine;

            {
                auto indent2 = stream.createIndentWithBraces();

                stream << "auto framesRemaining = context.numFrames - startFrame;" << newLine
                       << "auto numFramesToDo = framesRemaining < maxBlockSize ? framesRemaining : maxBlockSize;" << newLine;

                if (midiIns.empty())
                {
                    stream << "prepare (numFramesToDo);" << blankLine;
                }
                else
                {
                    stream << choc::text::trimStart (renderMIDIPreamble);

                    auto indent3 = stream.createIndentWithBraces();
                    stream << "auto midi = context.incomingMIDI.messages[startMIDIIndex++];" << newLine
                           << "auto packed = (static_cast<uint32_t> (midi.byte0) << 16) | (static_cast<uint32_t> (midi.byte1) << 8) | static_cast<uint32_t> (midi.byte2);" << newLine;

                    for (auto& input : findMIDIInputs())
                    {
                        auto fn = FunctionNames::addInputEvent (input, input->getSingleEventType());
                        stream << fn << " (state, { static_cast<int32_t> (packed) });" << newLine;
                    }
                }

                size_t inChanIndex = 0, outChanIndex = 0;

                for (auto& input : audioIns)
                {
                    auto numChans = input->getFrameType().getVectorSize();

                    stream << "copyToInterleaved (" << FunctionNames::getInputFrameArrayRef (input) << " (state).elements, &context.inputChannels["
                           << inChanIndex << "], startFrame, numFramesToDo);" << newLine;

                    inChanIndex += numChans;
                }

                stream << blankLine
                       << "advance();" << blankLine;

                for (auto& output : audioOuts)
                {
                    auto numChans = output->getFrameType().getVectorSize();

                    stream << "copyFromInterleaved (&context.outputChannels[" << outChanIndex << "], startFrame, "
                           << FunctionNames::getOutputFrameArrayRef (output) << " (state).elements, numFramesToDo);" << newLine;

                    outChanIndex += numChans;
                }

                stream << "startFrame += numFramesToDo;" << newLine;
            }

            stream << newLine;
        }
    }

    //==============================================================================
    decltype (Module::inputs) findAudioInputs() const
    {
        decltype (Module::inputs) results;

        for (auto& i : mainProcessor.inputs)
            if (i->isStreamEndpoint()
                  && ! isParameterInput (i->getDetails())
                  && i->getFrameType().isFloatingPoint()
                  && i->getFrameType().isPrimitiveOrVector())
                results.push_back (i);

        return results;
    }

    decltype (Module::outputs) findAudioOutputs() const
    {
        decltype (Module::outputs) results;

        for (auto& o : mainProcessor.outputs)
            if (o->isStreamEndpoint()
                  && o->getFrameType().isFloatingPoint()
                  && o->getFrameType().isPrimitiveOrVector())
                results.push_back (o);

        return results;
    }

    template <typename EndpointArray>
    static uint32_t getTotalAudioChannels (const EndpointArray& endpoints)
    {
        uint32_t total = 0;

        for (auto& e : endpoints)
            total += static_cast<uint32_t> (e->getFrameType().getVectorSize());

        return total;
    }

    decltype (Module::inputs) findMIDIInputs() const
    {
        decltype (Module::inputs) results;

        for (auto& i : mainProcessor.inputs)
            if (isMIDIEventEndpoint (i->getDetails()))
                results.push_back (i);

        return results;
    }

    //==============================================================================
    void printDirectPerformerMethods()
    {
        stream << sectionBreak
               << "// The following methods provide low-level access for read/write to all the" << newLine
               << "// endpoints directly, and to run the prepare/advance loop." << newLine
               << prepareAndAdvanceMethods;

        for (auto& input : mainProcessor.inputs)
        {
            auto details = input->getDetails();

            if (isStream (details))
            {
                {
                    stream << "void setNextInputStreamFrames_" << details.name
                           << " (" << getTypeWithConstness (input->getFrameOrValueType().createConstIfNotPresent())
                           << "* frames, uint32_t numFramesToUse)" << newLine;

                    auto indent = stream.createIndentWithBraces();
                    stream << "auto& buffer = " << FunctionNames::getInputFrameArrayRef (input) << " (state);" << blankLine
                           << "for (uint32_t i = 0; i < numFramesToUse; ++i)" << newLine
                           << "    buffer[static_cast<int> (i)] = frames[i];" << newLine;
                }

                stream << blankLine;

                stream << "void setNextInputStreamSparseFrames_" << details.name
                       << " (" << getTypeForParameter (input->getFrameOrValueType())
                       << " targetFrameValue, uint32_t numFramesToReachValue)" << newLine;

                auto indent = stream.createIndentWithBraces();
                stream << FunctionNames::setSparseInputTarget (input) << " (state, targetFrameValue, (int32_t) numFramesToReachValue);" << newLine;
            }
            else if (isEvent (details))
            {
                for (auto& type : input->dataTypes)
                {
                    stream << "void addInputEvent_" << details.name
                           << " (" << getTypeForParameter (type) << " eventValue)" << newLine;

                    auto indent = stream.createIndentWithBraces();
                    stream << FunctionNames::addInputEvent (input, type) << " (state, eventValue);" << newLine;
                }
            }
            else if (isValue (details))
            {
                stream << "void setInputValue_" << details.name
                       << " (" << getTypeForParameter (input->getValueType()) << " newValue)" << newLine;

                auto indent = stream.createIndentWithBraces();
                stream << FunctionNames::setInputValue (input) << " (state, newValue);" << newLine;
            }

            stream << blankLine;
        }

        for (auto& output : mainProcessor.outputs)
        {
            auto details = output->getDetails();

            if (isStream (details))
            {
                stream << "DynamicArray<" << getTypeWithConstness (output->getFrameOrValueType().createConstIfNotPresent())
                       << "> getOutputStreamFrames_" << details.name << "()" << newLine;

                auto indent = stream.createIndentWithBraces();
                stream << "return { &(" << FunctionNames::getOutputFrameArrayRef (output) << " (state).elements[0]), static_cast<int32_t> (framesToAdvance) };" << newLine;
            }
            else if (isEvent (details))
            {
                std::vector<std::string> paramTypes, paramNames;

                if (output->dataTypes.size() > 1)
                {
                    for (size_t i = 0; i < output->dataTypes.size(); ++i)
                        paramNames.push_back ("handleEventType" + std::to_string (i));
                }
                else
                {
                    paramNames.push_back ("handleEvent");
                }

                for (size_t i = 0; i < output->dataTypes.size(); ++i)
                    paramTypes.push_back ("std::function<bool(uint32_t frameOffset, " + getTypeForParameter (output->dataTypes[i]) + ")>&&");

                printFunctionWithMultiLineParamList ("void iterateOutputEvents_" + details.name, paramTypes, paramNames);

                auto indent = stream.createIndentWithBraces();
                stream << "auto numEvents = " << FunctionNames::getNumOutputEvents (output) << " (state);" << blankLine
                       << "for (int32_t i = 0; i < numEvents; ++i)" << newLine;

                {
                    auto indent2 = stream.createIndentWithBraces();

                    stream << "auto& event = " << FunctionNames::getOutputEventRef (output) << " (state, i);" << blankLine
                           << "switch (event.m_eventType)" << newLine;

                    {
                        auto indent3 = stream.createIndentWithBraces();
                        PaddedStringTable cases;

                        for (size_t typeIndex = 0; typeIndex < output->dataTypes.size(); ++typeIndex)
                        {
                            auto index = std::to_string (typeIndex);

                            cases.startRow();
                            cases.appendItem ("case " + index + ": ");
                            cases.appendItem (" if (! " + paramNames[typeIndex]);
                            cases.appendItem ("(static_cast<uint32_t> (event.m_eventTime), event.m_type" + index + ")) return;");
                            cases.appendItem ("break;");
                        }

                        cases.startRow();
                        cases.appendItem ("default:");
                        cases.appendItem (" SOUL_CPP_ASSERT (false);");

                        printTable (cases, 1000);
                    }

                    stream << newLine;
                }

                stream << newLine;
            }
            else if (isValue (details))
            {
                stream << getType (output->getValueType()) << " getOutputValue_" << details.name << "()" << newLine;

                auto indent = stream.createIndentWithBraces();
                stream << "return " << FunctionNames::getOutputValue (output) << " (state);" << newLine;
            }

            stream << blankLine;
        }
    }

    //==============================================================================
    void printPrivateContent()
    {
        stream << privateHelpers
               << sectionBreak;
        printStructs (false);
        stream << sectionBreak
               << choc::text::trimStart (warningsPush);

        for (auto& m : program.getModules())
        {
            stream << sectionBreak;
            printFunctions (m);
        }

        stream << warningsPop;
        printStringLookup();
        stream << sectionBreak
               << choc::text::trimStart (memberVariables);
        printExternalData();
    }

    //==============================================================================
    void printEndpointListMethods()
    {
        stream << sectionBreak
               << "// The following methods provide a fixed interface for finding out about" << newLine
               << "// the input/output endpoints that this processor provides." << newLine
               << endpointStruct;

        printEndpointList ("getInputEndpoints", mainProcessor.inputs);
        printEndpointList ("getOutputEndpoints", mainProcessor.outputs);
    }

    template <typename EndpointList>
    void printEndpointList (const std::string& functionName, const EndpointList& endpoints)
    {
        PaddedStringTable table;

        for (auto& e : endpoints)
        {
            auto details = e->getDetails();

            auto typeList = joinStrings (details.dataTypes, ", ", [] (auto& t) { return dump (t); });

            table.startRow();
            table.appendItem ("EndpointDetails {");
            table.appendItem (toCppStringLiteral (details.name, 150, false, false, false) + ",");
            table.appendItem (toCppStringLiteral (details.endpointID.toString(), 150, false, false, false) + ",");
            table.appendItem (std::string ("EndpointType::") + getEndpointTypeName (details.endpointType) + ",");
            table.appendItem (toCppStringLiteral (typeList, 150, false, false, false) + ",");
            table.appendItem (std::to_string (getNumAudioChannels (details)) + ",");
            table.appendItem (toCppStringLiteral (details.annotation.toJSON(), 150, false, false, false));
            table.appendItem ("}");
        }

        printFunctionReturningVector ("std::array<EndpointDetails, " + std::to_string (endpoints.size()) + "> " + functionName + "() const", table);
    }

    //==============================================================================
    void printPluginMethods()
    {
        stream << sectionBreak
               << "// The following methods provide help in dealing with the processor's endpoints" << newLine
               << "// in a format suitable for traditional audio plugin channels and parameters." << newLine
               << pluginStructs
               << blankLine
               << "static constexpr bool      hasMIDIInput = " << (findMIDIInputs().empty() ? "false" : "true") << ";" << newLine;

        printParameterPropertiesConstant();
        printAudioBusConstants();

        printCreateParametersMethod();
        printTimelineMethods();
    }

    std::vector<pool_ref<heart::InputDeclaration>> getParameterInputs()
    {
        std::vector<pool_ref<heart::InputDeclaration>> params;

        for (auto& i : mainProcessor.inputs)
            if (isParameterInput (i->getDetails()) && i->dataTypes.size() == 1 && i->dataTypes.front().isFloatingPoint())
                params.push_back (i);

        return params;
    }

    void printParameterPropertiesConstant()
    {
        auto numParameters = getParameterInputs().size();

        stream << "static constexpr uint32_t  numParameters = " << std::to_string (numParameters) << ";" << blankLine;

        PaddedStringTable table;
        table.numExtraSpaces = 2;

        for (auto& param : getParameterInputs())
        {
            auto details = param->getDetails();

            soul::patch::PatchParameterProperties props (details.name, details.annotation.toExternalValue());

            table.startRow();
            table.appendItem ("ParameterProperties {");
            table.appendItem (toCppStringLiteral (details.name, 150, false, false, false) + ",");
            table.appendItem (toCppStringLiteral (props.name, 150, false, false, false) + ",");
            table.appendItem (toCppStringLiteral (props.unit, 150, false, false, false) + ",");
            table.appendItem (choc::text::floatToString (props.minValue) + "f,");
            table.appendItem (choc::text::floatToString (props.maxValue) + "f,");
            table.appendItem (choc::text::floatToString (props.step) + "f,");
            table.appendItem (choc::text::floatToString (props.initialValue) + "f,");
            table.appendItem (props.isAutomatable ? "true," : "false,");
            table.appendItem (props.isBoolean     ? "true," : "false,");
            table.appendItem (props.isHidden      ? "true," : "false,");
            table.appendItem (toCppStringLiteral (props.group, 150, false, false, false) + ",");
            table.appendItem (toCppStringLiteral (props.textValues, 150, false, false, false));
            table.appendItem ("}");
        }

        if (numParameters != 0)
            printConstArray ("static constexpr const std::array<const ParameterProperties, numParameters> parameters", table);

        stream << "static span<const ParameterProperties> getParameterProperties() { return "
               << (numParameters != 0 ? "{ parameters.data(), numParameters }; }" : "{}; }") << blankLine;
    }

    void printCreateParametersMethod()
    {
        stream << choc::text::trimStart (parameterList) << blankLine;

        PaddedStringTable table;
        table.numExtraSpaces = 2;

        uint32_t paramIndex = 0;

        for (auto& param : getParameterInputs())
        {
            auto details = param->getDetails();

            soul::patch::PatchParameterProperties props (details.name, details.annotation.toExternalValue());

            table.startRow();
            table.appendItem ("Parameter {");
            table.appendItem ("parameters[" + std::to_string (paramIndex++) + "],");
            table.appendItem (choc::text::floatToString (props.initialValue) + "f,");

            if (isEvent (details))
                table.appendItem ("[this] (float v) { addInputEvent_" + details.name + " (v); }");
            else if (isValue (details))
                table.appendItem ("[this] (float v) { setInputValue_" + details.name + " (v); }");
            else if (isStream (details))
                table.appendItem ("[this] (float v) { setNextInputStreamSparseFrames_" + details.name
                                   + " (v, " + std::to_string (props.rampFrames) + "); }");
            else
                SOUL_ASSERT_FALSE;

            table.appendItem ("}");
        }

        printFunctionReturningVector ("ParameterList createParameterList()", table, true);
    }

    template <typename Test>
    std::vector<std::string> findTimelineEndpoints (Test&& test)
    {
        std::vector<std::string> results;

        for (auto& i : mainProcessor.inputs)
            if (i->isEventEndpoint() && i->dataTypes.size() == 1 && test (i->getSingleEventType().getExternalType()))
                results.push_back ("addInputEvent_" + i->name.toString());

        return results;
    }

    void printTimelineMethods()
    {
        auto timeSigEndpoints   = findTimelineEndpoints (TimelineEvents::isTimeSig);
        auto tempoEndpoints     = findTimelineEndpoints (TimelineEvents::isTempo);
        auto transportEndpoints = findTimelineEndpoints (TimelineEvents::isTransport);
        auto positionEndpoints  = findTimelineEndpoints (TimelineEvents::isPosition);

        stream << "static constexpr bool hasTimelineEndpoints = "
               << ((timeSigEndpoints.empty() && tempoEndpoints.empty() && transportEndpoints.empty() && positionEndpoints.empty())
                    ? "false" : "true") << ";" << blankLine;

        {
            stream << "void setTimeSignature (int32_t newNumerator, int32_t newDenominator)" << newLine;
            auto indent = stream.createIndentWithBraces();

            if (timeSigEndpoints.empty())
                stream << "(void) newNumerator; (void) newDenominator;" << newLine;

            for (auto& i : timeSigEndpoints)
                 stream << i << " ({ newNumerator, newDenominator });" << newLine;
        }

        stream << blankLine;

        {
            stream << "void setTempo (float newBPM)" << newLine;
            auto indent = stream.createIndentWithBraces();

            if (tempoEndpoints.empty())
                stream << "(void) newBPM;" << newLine;

            for (auto& i : tempoEndpoints)
                 stream << i << " ({ newBPM });" << newLine;
        }

        stream << blankLine;

        {
            stream << "void setTransportState (int32_t newState)" << newLine;
            auto indent = stream.createIndentWithBraces();

            if (transportEndpoints.empty())
                stream << "(void) newState;" << newLine;

            for (auto& i : transportEndpoints)
                 stream << i << " ({ newState });" << newLine;
        }

        stream << blankLine;

        {
            stream << "void setPosition (int64_t currentFrame, double currentQuarterNote, double lastBarStartQuarterNote)" << newLine;
            auto indent = stream.createIndentWithBraces();

            if (positionEndpoints.empty())
                stream << "(void) currentFrame; (void) currentQuarterNote; (void) lastBarStartQuarterNote;" << newLine;

            for (auto& i : positionEndpoints)
                 stream << i << " ({ currentFrame, currentQuarterNote, lastBarStartQuarterNote });" << newLine;
        }

        stream << blankLine;
    }

    void printAudioBusConstants()
    {
        PaddedStringTable inputs, outputs;

        for (auto& i : mainProcessor.inputs)
        {
            auto details = i->getDetails();

            if (auto numChans = getNumAudioChannels (details))
            {
                if (! isParameterInput (details))
                {
                    inputs.startRow();
                    inputs.appendItem ("AudioBus {");
                    inputs.appendItem (toCppStringLiteral (details.name, 150, false, false, false) + ",");
                    inputs.appendItem (std::to_string (numChans));
                    inputs.appendItem ("}");
                }
            }
        }

        for (auto& o : mainProcessor.outputs)
        {
            auto details = o->getDetails();

            if (auto numChans = getNumAudioChannels (details))
            {
                outputs.startRow();
                outputs.appendItem ("AudioBus {");
                outputs.appendItem (toCppStringLiteral (details.name, 150, false, false, false) + ",");
                outputs.appendItem (std::to_string (numChans));
                outputs.appendItem ("}");
            }
        }

        auto numInputBuses = inputs.getNumRows();
        auto numOutputBuses = outputs.getNumRows();

        stream << "static constexpr uint32_t numInputBuses  = " << numInputBuses << ";" << newLine
               << "static constexpr uint32_t numOutputBuses = " << numOutputBuses << ";" << blankLine;

        if (numInputBuses != 0)   printConstArray ("static constexpr std::array<const AudioBus, numInputBuses>  inputBuses", inputs);
        if (numOutputBuses != 0)  printConstArray ("static constexpr std::array<const AudioBus, numOutputBuses> outputBuses", outputs);

        stream << "static span<const AudioBus> getInputBuses()  { return " << (numInputBuses  != 0 ? "{ inputBuses.data(), numInputBuses }; }" : "{}; }") << newLine;
        stream << "static span<const AudioBus> getOutputBuses() { return " << (numOutputBuses != 0 ? "{ outputBuses.data(), numOutputBuses }; }" : "{}; }") << blankLine;
    }

    //==============================================================================
    void printFunctions (const soul::Module& m)
    {
        currentModule = &m;
        createUpcastFunctions();

        for (auto f : m.functions.get())
            if (f->functionType.isRun())
                printFunction (f);

        for (auto f : m.functions.get())
            if (f->isExported && ! f->functionType.isRun())
                printFunction (f);

        stream << sectionBreak;

        for (auto f : m.functions.get())
            if (! (f->functionType.isRun() || f->isExported))
                printFunction (f);

        currentModule = nullptr;
    }

    void printStringLookup()
    {
        stream << sectionBreak;
        auto& dictionary = program.getStringDictionary();

        if (dictionary.strings.empty())
        {
            stream << "// The program contains no string literals, so this function should never be called" << newLine
                   << "static constexpr const char* lookupStringLiteral (int32_t)  { return {}; }" << newLine;

            return;
        }

        stream << "static constexpr const char* lookupStringLiteral (int32_t handle)" << newLine;
        auto indent = stream.createIndentWithBraces();

        PaddedStringTable cases;

        for (auto& item : dictionary.strings)
        {
            cases.startRow();
            cases.appendItem ("case " + std::to_string (item.handle.handle) + ":");
            cases.appendItem (" return " + toCppStringLiteral (item.text, 150, true, false, true) + ";");
        }

        cases.startRow();
        cases.appendItem ("default:");
        cases.appendItem (" return {};");

        stream << "switch (handle)" << newLine;

        {
            auto indent2 = stream.createIndentWithBraces();
            printTable (cases, 1000);
        }

        stream << newLine;
    }

    static std::string makeSafeIdentifier (std::string name)
    {
        name = makeSafeIdentifierName (name);

        constexpr const char* reservedWords[] =
        {
            "alignas", "alignof", "and", "and_eq", "asm", "atomic_cancel", "atomic_commit", "atomic_noexcept", "auto",
            "bitand", "bitor", "bool", "break", "case", "catch", "char", "char8_t", "char16_t", "char32_t", "class",
            "compl", "concept", "const", "consteval", "constexpr", "constinit", "const_cast", "continue", "co_await",
            "co_return", "co_yield", "decltype", "default", "delete", "do", "double", "dynamic_cast", "else", "enum",
            "explicit", "export", "extern", "false", "float", "for", "friend", "goto", "if", "inline", "int", "long",
            "mutable", "namespace", "new", "noexcept", "not", "not_eq", "nullptr", "operator", "or", "or_eq", "private",
            "protected", "public", "reflexpr", "register", "reinterpret_cast", "requires", "return", "short", "signed",
            "sizeof", "static", "static_assert", "static_cast", "struct", "switch", "synchronized", "template", "this",
            "thread_local", "throw", "true", "try", "typedef", "typeid", "typename", "union", "unsigned", "using",
            "virtual", "void", "volatile", "wchar_t", "while", "xor", "xor_eq"
        };

        for (auto r : reservedWords)
            if (name == r)
                return name + "_";

        return name;
    }

    static std::string mangleStructOrFunctionName (const std::string& namespacedName)
    {
        return makeSafeIdentifier (choc::text::replace (Program::stripRootNamespaceFromQualifiedPath (namespacedName), ":", "_"));
    }

    struct ValueString
    {
        std::string text;
        bool needsBracketing = false;

        std::string getWithBracketsAlways() const          { return "(" + text + ")"; }
        std::string getWithBracketsIfNeeded() const        { return needsBracketing ? getWithBracketsAlways() : text; }
    };

    void printStructs (bool predeclare)
    {
        std::vector<StructurePtr> visited;

        for (auto& m : program.getModules())
            for (auto s : m->structs.get())
                printStructs (s, visited, predeclare);
    }

    void printStaticConstants()
    {
        if (! options.staticConstants.isEmpty())
        {
            PaddedStringTable table;

            for (auto& name : options.staticConstants.getNames())
            {
                auto value = options.staticConstants.getValue (name);

                table.startRow();

                table.appendItem ("static constexpr " + getType (value.getType().createConstIfNotPresent(), {}, "const char*"));
                table.appendItem (" " + name);
                table.appendItem (" = " + getConstantString (value, std::addressof (options.staticConstants.getDictionary())) + ";");
            }

            printTable (table, 200);
            stream << sectionBreak;
        }
    }

    void createUpcastFunctions()
    {
        struct UpcastFunction
        {
            Type dest, source;

            bool operator== (const UpcastFunction& other) const
            {
                return dest.isIdentical (other.dest) && source.isIdentical (other.source);
            }
        };

        std::vector<UpcastFunction> upcastFunctions;

        for (auto& f : currentModule->functions.get())
        {
            f->visitExpressions ([this, &upcastFunctions] (pool_ref<heart::Expression>& value, AccessType mode)
            {
                if (mode == AccessType::read)
                {
                    if (auto c = cast<heart::TypeCast> (value))
                    {
                        auto sourceType = c->source->getType();

                        if (! sourceType.isEqual (c->destType, Type::ignoreReferences | Type::ignoreConst | Type::ignoreVectorSize1)
                             && c->destType.isStruct() && sourceType.isStruct())
                        {
                            if (! contains (upcastFunctions, UpcastFunction { c->destType, sourceType }))
                            {
                                // Determine the index of the target in the source type
                                size_t index;

                                if (canUpcastTypes (c->destType, sourceType, index))
                                {
                                    stream << "static " << getType (c->destType) << " _stateUpCast (" << getType (sourceType) << " s)" << newLine;
                                    stream << "{" << newLine;

                                    {
                                        auto stateMemberName = mangleStructMemberName (c->destType.getStructRef().getMemberName (index));
                                        auto indent = stream.createIndent();

                                        if (c->destType.getStructRef().getMemberType (index).isArray())
                                        {
                                            auto arrayType = c->destType.getStructRef().getMemberType (index);

                                            stream << "auto offset = static_cast<int32_t> (offsetof (" << getType (c->destType.removeReferenceIfPresent())
                                                   << ", " << stateMemberName << ") + "
                                                   << getType (arrayType) << "::elementOffset (s.m__arrayEntry));" << newLine;
                                        }
                                        else
                                        {
                                            stream << "auto offset = static_cast<int32_t> (offsetof (" << getType (c->destType.removeReferenceIfPresent())
                                                   << ", " << stateMemberName << "));" << newLine;
                                        }

                                        stream << "return *reinterpret_cast<" << getType (c->destType.removeReferenceIfPresent()) << "*> (reinterpret_cast<char*> (&s) - offset);" << newLine;
                                    }

                                    stream << "}" << blankLine;
                                }
                                else
                                {
                                    SOUL_ASSERT_FALSE;
                                }

                                upcastFunctions.push_back (UpcastFunction { c->destType, sourceType });
                            }
                        }
                    }
                }
            });
        }
    }

    void printFunction (const heart::Function& f)
    {
        localVariableNames.clear();

        stream << getType (f.returnType) << " " << getFunctionName (f);

        if (f.parameters.empty())
        {
            stream << "(";
        }
        else
        {
            stream << " (";
            bool first = true;

            for (auto& p : f.parameters)
            {
                if (first)
                    first = false;
                else
                    stream << ", ";

                stream << getTypeWithConstness (p->type) << " " << getLocalVariableName (p);
            }
        }

        stream << ") noexcept";

        if (f.hasNoBody)
        {
            stream << ";";
        }
        else
        {
            stream << newLine
                   << "{" << newLine;

            {
                auto indent = stream.createIndent();
                printLocalVariableDeclarations (f);
                bool needsBraces = f.blocks.size() != 1;

                for (size_t i = 0; i < f.blocks.size() - 1; ++i)
                    printBlock (f.blocks, f.blocks[i], f.blocks[i + 1].getPointer(), i != 0 && needsBraces);

                printBlock (f.blocks, f.blocks.back(), nullptr, needsBraces);
            }

            stream << "}";
        }

        stream << blankLine;
    }

    void printStructs (StructurePtr s, std::vector<StructurePtr>& visited, bool predeclare)
    {
        if (! contains (visited, s))
        {
            visited.push_back (s);

            for (auto& m : s->getMembers())
                printStructs (m.type, visited, predeclare);

            printStruct (*s, predeclare);
        }
    }

    void printStructs (const Type& type, std::vector<StructurePtr>& visited, bool predeclare)
    {
        if (type.isStruct())
            printStructs (type.getStruct(), visited, predeclare);
        else if (type.isArray())
            printStructs (type.getArrayElementType(), visited, predeclare);
    }

    void printStruct (const soul::Structure& s, bool predeclare)
    {
        auto structName = getStructName (s);
        stream << "struct " << structName;

        if (predeclare)
        {
            stream << ";" << newLine;
            return;
        }

        stream << newLine
               << "{" << newLine;

        struct Member
        {
            std::string type;
            std::vector<std::string> names;
        };

        std::vector<Member> members;
        std::string lastType;

        for (auto& m : s.getMembers())
        {
            auto type = getType (m.type);
            auto name = mangleStructMemberName (m.name);

            if (lastType == type && ! (containsChar (type, '*') || containsChar (type, '&')))
            {
                members.back().names.push_back (name);
            }
            else
            {
                lastType = type;

                Member member;
                member.type = std::move (type);
                member.names.push_back (std::move (name));
                members.push_back (std::move (member));
            }
        }

        {
            auto indent = stream.createIndent();

            for (auto& m : members)
                stream << m.type << " " << choc::text::joinStrings (m.names, ", ") << ";" << newLine;
        }

        stream << "};" << blankLine;
    }

    void printLocalVariableDeclarations (const heart::Function& f)
    {
        auto functionLocals = f.getAllLocalVariables();

        // Include block parameters
        for (auto& b : f.blocks)
        {
            for (auto& param : b->parameters)
            {
                functionLocals.push_back (param);
            }
        }

        heart::Utilities::VariableListByType locals (functionLocals);

        bool anyPrinted = false;

        for (auto& type : locals.types)
        {
            bool hasPrintedType = false;

            for (auto& v : type.variables)
            {
                // Only forward declare non-reference types
                if (! type.type.isReference())
                {
                    if (! hasPrintedType)
                    {
                        hasPrintedType = true;
                        stream << getType (type.type) << " ";
                    }
                    else
                    {
                        stream << ", ";
                    }

                    stream << getLocalVariableName (v) << " = {}";
                    anyPrinted = true;
                }
            }

            if (hasPrintedType)
                stream << ";" << newLine;
        }

        if (anyPrinted)
            stream << blankLine;
    }

    static bool anyBlockJumpsTo (choc::span<pool_ref<heart::Block>> allBlocks, heart::Block& target)
    {
        for (auto& b : allBlocks)
            if (contains (b->terminator->getDestinationBlocks(), target))
                return true;

        return false;
    }

    void printBlock (choc::span<pool_ref<heart::Block>> allBlocks, heart::Block& b,
                     const heart::Block* nextBlock, bool needsBraces)
    {
        size_t labelLength = 0;

        if (anyBlockJumpsTo (allBlocks, b))
        {
            stream << getBlockName (b) << ": ";
            needsBraces = true;
            labelLength = getBlockName (b).length() + 2;
        }

        bool needsTerminator = true;

        if (auto tb = cast<const heart::Branch> (b.terminator))
            needsTerminator = (nextBlock == nullptr || *nextBlock != tb->target || (! tb->targetArgs.empty()));
        else
            needsTerminator = nextBlock != nullptr || ! is_type<const heart::ReturnVoid> (b.terminator);

        if (needsBraces)
        {
            if (b.statements.empty())
            {
                if (needsTerminator)
                {
                    stream << "{ ";
                    printTerminator (*b.terminator, nextBlock);
                    stream << " ";
                }
                else
                {
                    stream << "{";
                }
            }
            else if (! needsTerminator && b.statements.begin().next() == nullptr)
            {
                stream << "{ ";
                printStatement (**b.statements.begin());
                stream << " ";
            }
            else
            {
                stream << "{ ";

                {
                    auto s = b.statements.begin();
                    auto end = b.statements.end();

                    if (printStatement (**s))
                    {
                        ++s;
                        stream << newLine;
                    }

                    auto statementIndent = stream.createIndent (labelLength + 2);

                    for (; s != end; ++s)
                        if (printStatement (**s))
                            stream << newLine;

                    if (needsTerminator)
                    {
                        if (printTerminator (*b.terminator, nextBlock))
                            stream << newLine;
                    }
                }
            }

            stream << "}" << newLine;
        }
        else
        {
            for (auto s : b.statements)
                if (printStatement (*s))
                    stream << newLine;

            if (needsTerminator)
            {
                printTerminator (*b.terminator, nextBlock);
                stream << newLine;
            }
        }
    }

    bool printStatement (const heart::Statement& s)
    {
        if (auto a = cast<const heart::AssignFromValue> (s))
            return printAssignment (*a->target, a->source);

        if (auto fc = cast<const heart::FunctionCall> (s))
            return printFunctionCall (*fc);

        SOUL_ASSERT_FALSE;
    }

    template <typename ArgListType>
    void printBlockParameterAssignments (std::vector<pool_ref<soul::heart::Variable>>& parameters, ArgListType& args)
    {
        SOUL_ASSERT (parameters.size() == args.size());

        for (size_t param = 0; param < parameters.size(); param++)
        {
            printAssignment (parameters[param], args[param]);
            stream << newLine;
        }
    }

    bool printTerminator (const heart::Terminator& t, const heart::Block* nextBlock)
    {
        if (auto b = cast<const heart::Branch> (t))
        {
            printBlockParameterAssignments (b->target->parameters, b->targetArgs);

            if (nextBlock == nullptr || *nextBlock != b->target)
                stream << "goto " << getBlockName (b->target) << ";";
            else
                return false;

            return true;
        }

        if (auto b = cast<const heart::BranchIf> (t))
        {
            if (nextBlock != nullptr && *nextBlock == b->targets[0])
            {
                stream << "if (! " << getValue (b->condition).getWithBracketsIfNeeded()
                       << ") goto " << getBlockName (b->targets[1]) << ";";
                return true;
            }

            stream << "if " << getValue (b->condition).getWithBracketsAlways()
                   << " goto " << getBlockName (b->targets[0]) << ";";

            if (nextBlock == nullptr || *nextBlock != b->targets[1])
                stream << newLine << "goto " << getBlockName (b->targets[1]) << ";";

            return true;
        }

        if (is_type<const heart::ReturnVoid> (t))
        {
            if (nextBlock != nullptr)
                stream << "return;";

            return true;
        }

        if (auto r = cast<const heart::ReturnValue> (t))
        {
            stream << "return " << getValue (r->returnValue).text << ";";
            return true;
        }

        SOUL_ASSERT_FALSE;
    }

    bool printAssignment (heart::Expression& dest, heart::Expression& source)
    {
        bool isParameter = false;

        if (auto r = cast<soul::heart::Variable> (dest))
            isParameter = r->isParameter();

        if (! isParameter && dest.getType().isReference())
            stream << "auto& ";

        stream << getValue (dest).text << " = " << getValue (source).text << ";";
        return true;
    }

    ValueString printUnaryOp (const heart::UnaryOperator& op)
    {
        if (op.operation == UnaryOp::Op::negate)      return { "-"  + getValue (op.source).getWithBracketsIfNeeded(), true };
        if (op.operation == UnaryOp::Op::logicalNot)  return { "! " + getValue (op.source).getWithBracketsIfNeeded(), true };
        if (op.operation == UnaryOp::Op::bitwiseNot)  return { "~"  + getValue (op.source).getWithBracketsIfNeeded(), true };

        SOUL_ASSERT_FALSE;
        return {};
    }

    ValueString printBinaryOp (const char* operatorText, const heart::Expression& lhs, const heart::Expression& rhs)
    {
        return { getValue (lhs).getWithBracketsIfNeeded() + " " + operatorText + " " + getValue (rhs).getWithBracketsIfNeeded(), true };
    }

    ValueString printUnsignedBinaryOp (const char* operatorText, const heart::Expression& lhs, const heart::Expression& rhs)
    {
        if (lhs.getType().isInteger())
            return { "static_cast<uint32_t> " + getValue (lhs).getWithBracketsAlways() + " "
                        + operatorText + " " + getValue (rhs).getWithBracketsIfNeeded(), true };

        return printBinaryOp (operatorText, lhs, rhs);
    }

    ValueString printBinaryFunc (std::string functionName, const heart::Expression& lhs, const heart::Expression& rhs)
    {
        return { functionName + " (" + getValue (lhs).text + ", " + getValue (rhs).text + ")", false };
    }

    ValueString printBinaryOp (const heart::BinaryOperator& op)
    {
        auto& lhs = op.lhs.get();
        auto& rhs = op.rhs.get();
        using Op = BinaryOp::Op;

        switch (op.operation)
        {
            case Op::add:                return printBinaryOp ("+",   lhs, rhs);
            case Op::subtract:           return printBinaryOp ("-",   lhs, rhs);
            case Op::multiply:           return printBinaryOp ("*",   lhs, rhs);
            case Op::divide:             return printBinaryOp ("/",   lhs, rhs);
            case Op::modulo:             return lhs.getType().isFloatingPoint() ? printBinaryFunc ("SOUL_INTRINSICS::fmod", lhs, rhs)
                                                                                : printBinaryOp ("%", lhs, rhs);
            case Op::bitwiseOr:          return printBinaryOp ("|",   lhs, rhs);
            case Op::bitwiseAnd:         return printBinaryOp ("&",   lhs, rhs);
            case Op::bitwiseXor:         return printBinaryOp ("^",   lhs, rhs);
            case Op::leftShift:          return printBinaryOp ("<<",  lhs, rhs);
            case Op::rightShift:         return printBinaryOp (">>",  lhs, rhs);
            case Op::rightShiftUnsigned: return printUnsignedBinaryOp (">>",  lhs, rhs);

            case Op::equals:             return printBinaryOp ("==",  lhs, rhs);
            case Op::notEquals:          return printBinaryOp ("!=",  lhs, rhs);
            case Op::lessThan:           return printBinaryOp ("<",   lhs, rhs);
            case Op::lessThanOrEqual:    return printBinaryOp ("<=",  lhs, rhs);
            case Op::greaterThan:        return printBinaryOp (">",   lhs, rhs);
            case Op::greaterThanOrEqual: return printBinaryOp (">=",  lhs, rhs);

            case Op::logicalOr:          return printBinaryOp ("||",  lhs, rhs);
            case Op::logicalAnd:         return printBinaryOp ("&&",  lhs, rhs);

            case Op::unknown:
            default:                     SOUL_ASSERT_FALSE; return {};
        }
    }

    std::string printPureFunctionCall (const heart::PureFunctionCall& fc)
    {
        if (fc.function.functionType.isIntrinsic())
            return createIntrinsicCall (fc.function, fc.arguments);

        return getFunctionName (fc.function) + createArgList (fc.arguments);
    }

    std::string printProcessorProperty (const heart::ProcessorProperty& p)
    {
        SOUL_ASSERT (currentModule != nullptr);

        switch (p.property)
        {
            case heart::ProcessorProperty::Property::frequency:   return "(sampleRate * " + choc::text::floatToString (currentModule->sampleRate) + ")";
            case heart::ProcessorProperty::Property::period:      return "(1.0 / (sampleRate * " + choc::text::floatToString (currentModule->sampleRate) + "))";
            case heart::ProcessorProperty::Property::latency:     return std::to_string (currentModule->latency);

            case heart::ProcessorProperty::Property::none:
            case heart::ProcessorProperty::Property::id:
            case heart::ProcessorProperty::Property::session:
            default:                                              SOUL_ASSERT_FALSE; break;
        }

        return {};
    }

    static bool canUpcastTypes (const soul::Type& parentType, const soul::Type& childType, size_t& index)
    {
        index = 0;

        if (parentType.isStruct())
        {
            auto parentStructPtr = parentType.getStruct();
            auto childTypeDereferenced = childType.removeReferenceIfPresent();

            for (auto& m : parentStructPtr->getMembers())
            {
                if (m.type.isIdentical (childTypeDereferenced)
                     || (m.type.isArray() && m.type.getArrayElementType().isIdentical (childTypeDereferenced)))
                    return true;

                ++index;
            }
        }

        return false;
    }

    ValueString printCast (const heart::TypeCast& c)
    {
        const auto& source = c.source.get();
        const auto& sourceType = source.getType();
        auto castType = TypeRules::getCastType (c.destType, sourceType);

        switch (castType)
        {
            case TypeRules::CastType::identity:
            case TypeRules::CastType::primitiveNumericLossless:
            case TypeRules::CastType::primitiveNumericReduction:
                return { "static_cast<" + getType (c.destType) + "> " + getValue (source).getWithBracketsAlways(), false };

            case TypeRules::CastType::valueToArray:
            case TypeRules::CastType::arrayElementLossless:
            case TypeRules::CastType::arrayElementReduction:
                if (c.destType.isVector())
                    return { getType (c.destType) + " (" + getValue (source).text + ")", false };

                return getValue (source);

            case TypeRules::CastType::singleElementVectorToScalar:
                return { getValue (source).getWithBracketsIfNeeded() + "[0]", false };

            case TypeRules::CastType::wrapValue:
                return { "_intrin_wrap (static_cast<int32_t> (" + getValue (source).text + "), " + std::to_string (c.destType.getBoundedIntLimit()) + ")", false };

            case TypeRules::CastType::clampValue:
                return { "_intrin_clamp (static_cast<int32_t> (" + getValue (source).text + "), 0, " + std::to_string (c.destType.getBoundedIntLimit()) + ")", false };

            case TypeRules::CastType::fixedSizeArrayToDynamicArray:
                SOUL_ASSERT (sourceType.isFixedSizeArray());
                return { getValue (source).text + ".toDynamicArray()", false };

            case TypeRules::CastType::notPossible:
                {
                    size_t index;

                    if (canUpcastTypes (c.destType, sourceType, index))
                        return { "_stateUpCast " + getValue (source).getWithBracketsAlways(), false };

                    SOUL_ASSERT_FALSE;
                    return {};
                }

            default:
                SOUL_ASSERT_FALSE;
                return {};
        }
    }

    bool printFunctionCall (const heart::FunctionCall& fc)
    {
        auto& function = fc.getFunction();

        if (function.functionType.isIntrinsic())
        {
            if (fc.target == nullptr)
                return false; // these are all pure, so this is a NOP

            stream << getValue (*fc.target).text << " = " << createIntrinsicCall (function, fc.arguments) << ";";
        }
        else
        {
            if (fc.target != nullptr)
                stream << getValue (*fc.target).text << " = ";

            stream << getFunctionName (fc.getFunction()) << createArgList (fc.arguments) << ";";
        }

        return true;
    }

    std::string createIntrinsicCall (const heart::Function& f, choc::span<pool_ref<heart::Expression>> args)
    {
        SOUL_ASSERT (f.functionType.isIntrinsic());

        switch (f.intrinsicType)
        {
            case IntrinsicType::sqrt:           return createIntrinsicCall ("SOUL_INTRINSICS::sqrt",  "_vec_sqrt",   args);
            case IntrinsicType::pow:            return createIntrinsicCall ("SOUL_INTRINSICS::pow",   "_vec_pow",    args);
            case IntrinsicType::exp:            return createIntrinsicCall ("SOUL_INTRINSICS::exp",   "_vec_exp",    args);
            case IntrinsicType::log:            return createIntrinsicCall ("SOUL_INTRINSICS::log",   "_vec_log",    args);
            case IntrinsicType::log10:          return createIntrinsicCall ("SOUL_INTRINSICS::log10", "_vec_log10",  args);
            case IntrinsicType::sin:            return createIntrinsicCall ("SOUL_INTRINSICS::sin",   "_vec_sin",    args);
            case IntrinsicType::cos:            return createIntrinsicCall ("SOUL_INTRINSICS::cos",   "_vec_cos",    args);
            case IntrinsicType::tan:            return createIntrinsicCall ("SOUL_INTRINSICS::tan",   "_vec_tan",    args);
            case IntrinsicType::sinh:           return createIntrinsicCall ("SOUL_INTRINSICS::sinh",  "_vec_sinh",   args);
            case IntrinsicType::cosh:           return createIntrinsicCall ("SOUL_INTRINSICS::cosh",  "_vec_cosh",   args);
            case IntrinsicType::tanh:           return createIntrinsicCall ("SOUL_INTRINSICS::tanh",  "_vec_tanh",   args);
            case IntrinsicType::asinh:          return createIntrinsicCall ("SOUL_INTRINSICS::asinh", "_vec_asinh",  args);
            case IntrinsicType::acosh:          return createIntrinsicCall ("SOUL_INTRINSICS::acosh", "_vec_acosh",  args);
            case IntrinsicType::atanh:          return createIntrinsicCall ("SOUL_INTRINSICS::atanh", "_vec_atanh",  args);
            case IntrinsicType::asin:           return createIntrinsicCall ("SOUL_INTRINSICS::asin",  "_vec_asin",   args);
            case IntrinsicType::acos:           return createIntrinsicCall ("SOUL_INTRINSICS::acos",  "_vec_acos",   args);
            case IntrinsicType::atan:           return createIntrinsicCall ("SOUL_INTRINSICS::atan",  "_vec_atan",   args);
            case IntrinsicType::atan2:          return createIntrinsicCall ("SOUL_INTRINSICS::atan2", "_vec_atan2",  args);
            case IntrinsicType::isnan:          return createIntrinsicCall ("SOUL_INTRINSICS::isnan", args);
            case IntrinsicType::isinf:          return createIntrinsicCall ("SOUL_INTRINSICS::isinf", args);
            case IntrinsicType::get_array_size: return getValue (args.front()).text + ".numElements";

            case IntrinsicType::none:
            case IntrinsicType::abs:
            case IntrinsicType::min:
            case IntrinsicType::max:
            case IntrinsicType::clamp:
            case IntrinsicType::wrap:
            case IntrinsicType::fmod:
            case IntrinsicType::remainder:
            case IntrinsicType::floor:
            case IntrinsicType::ceil:
            case IntrinsicType::roundToInt:
            case IntrinsicType::addModulo2Pi:
            case IntrinsicType::sum:
            case IntrinsicType::product:
            case IntrinsicType::read:
            case IntrinsicType::readLinearInterpolated:
            default:                            return createIntrinsicCall (getFunctionName (f), args);
        }
    }

    std::string createIntrinsicCall (const char* scalarFunction, const char* vectorFunction,
                                     choc::span<pool_ref<heart::Expression>> args)
    {
        const auto& argType = args.front()->getType();
        return (argType.isVector() ? vectorFunction : scalarFunction) + createArgList (args);
    }

    std::string createIntrinsicCall (const std::string& functionName, choc::span<pool_ref<heart::Expression>> args)
    {
        return functionName + createArgList (args);
    }

    std::string createArgList (choc::span<pool_ref<heart::Expression>> args)
    {
        if (args.empty())
            return "()";

        std::string s = " (";
        bool first = true;

        for (auto& a : args)
        {
            if (first)
                first = false;
            else
                s += ", ";

            s += getValue (a).text;
        }

        return s + ")";
    }

    std::string getType (const Type& type, std::string namespaceToUse = {}, const char* stringLiteralType = "StringLiteral")
    {
        auto prefix = namespaceToUse.empty() ? "" : namespaceToUse + "::";

        if (type.isVoid())           return "void";
        if (type.isReference())      return getType (type.removeReference(), {}, stringLiteralType) + "&";
        if (type.isPrimitive())      return getType (type.getPrimitiveType());
        if (type.isStruct())         return prefix + getStructName (type.getStructRef());
        if (type.isVector())         return prefix + "Vector<" + getType (type.getVectorElementType(), namespaceToUse, stringLiteralType) + ", " + std::to_string (type.getVectorSize()) + ">";
        if (type.isUnsizedArray())   return prefix + "DynamicArray<" + getType (type.getArrayElementType(), namespaceToUse, stringLiteralType) + ">";
        if (type.isArray())          return prefix + "FixedArray<" + getType (type.getArrayElementType(), namespaceToUse, stringLiteralType)  + ", " + std::to_string (type.getArraySize()) + ">";
        if (type.isBoundedInt())     return getType (Type::getBoundedIntSizeType(), namespaceToUse, stringLiteralType);
        if (type.isStringLiteral())  return stringLiteralType;

        SOUL_ASSERT_FALSE;
        return {};
    }

    std::string getType (PrimitiveType type)
    {
        switch (type.type)
        {
            case PrimitiveType::Primitive::void_:     return "void";
            case PrimitiveType::Primitive::float32:   return "float";
            case PrimitiveType::Primitive::float64:   return "double";
            case PrimitiveType::Primitive::fixed:     return "fixed";
            case PrimitiveType::Primitive::complex32: return "complex32";
            case PrimitiveType::Primitive::complex64: return "complex64";
            case PrimitiveType::Primitive::int32:     return "int32_t";
            case PrimitiveType::Primitive::int64:     return "int64_t";
            case PrimitiveType::Primitive::bool_:     return "bool";

            case PrimitiveType::invalid:
            default:
                SOUL_ASSERT_FALSE;
                return "<unknown>";
        }
    }

    std::string getTypeWithConstness (const Type& type)
    {
        if (type.isConst())
            return "const " + getType (type.removeConst());

        return getType (type);
    }

    std::string getTypeForParameter (const Type& type)
    {
        if (type.isPrimitiveOrVector() || type.isStringLiteral())
            return getTypeWithConstness (type);

        return getTypeWithConstness (type.withConstAndRefFlags (true, true));
    }

    std::string getCastToTypeFromVoidPointer (const Type& type, const std::string& source)
    {
        return "*(" + getTypeWithConstness (type.removeReferenceIfPresent()) + "*) " + source;
    }

    static std::string getBlockName (const heart::Block& block)
    {
        return choc::text::replace ("_" + block.name.toString(), "@", {});
    }

    std::string getConstantString (const Value& v, const StringDictionary* dictionaryToUse)
    {
        struct PrintConstant  : public ValuePrinter
        {
            PrintConstant (CPPGenerator& c) : cppGen (c) {}

            CPPGenerator& cppGen;
            std::ostringstream out;

            void print (std::string_view s) override           { out << s; }

            void printZeroInitialiser (const Type&) override   { print ("ZeroInitialiser()"); }

            void beginArrayMembers (const Type& t) override    { print (cppGen.getType (t) + " { { "); }
            void endArrayMembers() override                    { print (" } }"); }

            void beginVectorMembers (const Type& t) override   { print (cppGen.getType (t) + " { "); }
            void endVectorMembers() override                   { print (" }"); }

            void beginStructMembers (const Type& t) override   { print (cppGen.getType (t) + " { "); }
            void endStructMembers() override                   { print (" }"); }

            void printUnsizedArrayContent (const Type& arrayType, const void* pointer) override
            {
                ConstantTable::Handle handle;
                writeUnaligned (std::addressof (handle), pointer);

                if (handle == 0)
                    return print (cppGen.getType (arrayType) + "()");

                if (auto value = cppGen.program.getConstantTable().getValueForHandle (handle))
                    return print (cppGen.getType (arrayType) + " { " + cppGen.getExternalDataVariable (handle)
                                    + ", (size_t) " + std::to_string (value->getType().getArraySize()) + " }");

                SOUL_ASSERT_FALSE;
            }

            void printStringLiteral (StringDictionary::Handle h) override
            {
                print (dictionary != nullptr ? toCppStringLiteral (std::string (dictionary->getStringForHandle (h)),
                                                                   200, true, false, true)
                                             : std::to_string (h.handle));
            }
        };

        PrintConstant pc (*this);
        pc.dictionary = dictionaryToUse;
        v.print (pc);
        return pc.out.str();
    }

    ValueString getValue (const heart::Expression& e)
    {
        auto constValue = e.getAsConstant();

        if (constValue.isValid())
            return { getConstantString (constValue, nullptr), false };

        if (auto s = cast<const heart::ArrayElement> (e))
        {
            auto parentValue = getValue (s->parent).getWithBracketsIfNeeded();

            if (s->dynamicIndex != nullptr)
                return { parentValue + "[" + getValue (*s->dynamicIndex).text + "]", false };

            if (s->isSingleElement())
                return { parentValue + "[" + std::to_string (s->fixedStartIndex) + "]", false };

            return { parentValue + ".slice<"
                       + std::to_string (s->fixedStartIndex) + ", "
                       + std::to_string (s->fixedEndIndex) + ">()", false };
        }

        if (auto s = cast<const heart::StructElement> (e))
        {
            auto parentValue = getValue (s->parent).getWithBracketsIfNeeded();
            return { parentValue + "." + mangleStructMemberName (s->memberName), false };
        }

        if (auto v = cast<const heart::Variable> (e))               return { getLocalVariableName (*v), false };
        if (auto c = cast<const heart::TypeCast> (e))               return printCast (*c);
        if (auto u = cast<const heart::UnaryOperator> (e))          return printUnaryOp (*u);
        if (auto b = cast<const heart::BinaryOperator> (e))         return printBinaryOp (*b);
        if (auto f = cast<const heart::PureFunctionCall> (e))       return { printPureFunctionCall (*f), false };
        if (auto p = cast<const heart::ProcessorProperty> (e))      return { printProcessorProperty (*p), false };

        SOUL_ASSERT_FALSE;
        return {};
    }

    std::string getStructName (const Structure& s)
    {
        return mangleStructOrFunctionName (program.getStructNameWithQualificationIfNeeded (mainProcessor, s));
    }

    std::string getFunctionName (const heart::Function& f)
    {
        return mangleStructOrFunctionName (program.getFunctionNameWithQualificationIfNeeded (mainProcessor, f));
    }

    std::string getLocalVariableName (const heart::Variable& v)
    {
        if (v.isExternal())
            return getExternalDataVariable (v.externalHandle);

        auto& saved = localVariableNames[v];

        if (! saved.empty())
            return saved;

        std::string uniqueName ("_");

        if (v.name.isValid())
            uniqueName = makeSafeIdentifier (v.name);

        if (localVariableNames.size() > 50)
            uniqueName += "_" + std::to_string (localVariableNames.size() + 1);
        else
            uniqueName = addSuffixToMakeUnique (v.name.isValid() ? makeSafeIdentifier (v.name) : std::string(),
                                                [&] (const std::string& nm)
                                                {
                                                    for (auto& lv : localVariableNames)
                                                        if (lv.second == nm)
                                                            return true;

                                                    return false;
                                                });
        saved = uniqueName;
        return uniqueName;
    }

    std::string getExternalDataVariable (ConstantTable::Handle handle)
    {
        for (auto& f : externalDataFunctions)
            if (f.handle == handle)
                return f.name;

        auto name = "_external_" + std::to_string (handle);

        if (auto value = program.getConstantTable().getValueForHandle (handle))
        {
            auto valueString = getConstantString (*value, nullptr);
            externalDataFunctions.push_back ({ handle, value->getType(), std::move (name), std::move (valueString) });
        }
        else
        {
            SOUL_ASSERT_FALSE;
        }

        return externalDataFunctions.back().name;
    }

    void printExternalData()
    {
        if (! externalDataFunctions.empty())
        {
            stream << sectionBreak;

            for (auto& e : externalDataFunctions)
                stream << "static inline const auto " << e.name << " = " << e.value << ";" << newLine;

            stream << sectionBreak;
        }
    }

    static std::string mangleStructMemberName (const std::string& name)
    {
        return "m_" + makeSafeIdentifier (name);
    }

    void printTable (PaddedStringTable& table, size_t maxLineLength = 1000)
    {
        auto oldMaxLineLen = stream.getLineWrapLength();
        stream.setLineWrapLength (maxLineLength);
        table.iterateRows ([&] (const std::string& s) { stream << s << newLine; });
        stream.setLineWrapLength (oldMaxLineLen);
    }

    void addCommaSeparatorsToTableRows (PaddedStringTable& table)
    {
        for (size_t i = 0; i < table.getNumRows() - 1; ++i)
            table.getCell (i, table.getNumColumns(i) - 1) += ",";
    }

    void printConstArray (const std::string& variableDecl, PaddedStringTable& table)
    {
        if (table.getNumRows() == 0)
        {
            stream << variableDecl << " = {};" << blankLine;
            return;
        }

        if (table.getNumRows() == 1 && table.getRow (0).length() < 50)
        {
            stream << variableDecl << " = { " << table.getRow (0) << " };" << blankLine;
            return;
        }

        stream << variableDecl << " = " << newLine;

        {
            auto indent1 = stream.createIndentWithBraces();
            addCommaSeparatorsToTableRows (table);
            printTable (table, 1000);
        }

        stream << ";" << blankLine;
    }

    void printFunctionReturningVector (const std::string& functionDecl, PaddedStringTable& table,
                                       bool addExtraBraceLevel = false)
    {
        if (table.getNumRows() == 0)
        {
            stream << functionDecl << "   { return {}; }" << blankLine;
            return;
        }

        if (table.getNumRows() == 1 && table.getRow (0).length() < 50)
        {
            stream << functionDecl << "   { return { "
                   << (addExtraBraceLevel ? "{ " : "")
                   << table.getRow (0)
                   << (addExtraBraceLevel ? " }" : "")
                   << " }; }" << blankLine;
            return;
        }

        stream << functionDecl << newLine;

        {
            auto indent1 = stream.createIndentWithBraces();

            stream << "return" << newLine;

            {
                std::unique_ptr<choc::text::CodePrinter::Indent> extraBrace;

                if (addExtraBraceLevel)
                    extraBrace = std::make_unique<choc::text::CodePrinter::Indent> (stream.createIndentWithBraces());

                {
                    auto indent2 = stream.createIndentWithBraces();
                    addCommaSeparatorsToTableRows (table);
                    printTable (table, 1000);
                }

                if (addExtraBraceLevel)
                    stream << newLine;
            }

            stream << ";" << newLine;
        }

        stream << blankLine;
    }

    void printFunctionWithMultiLineParamList (const std::string functionDecl,
                                              choc::span<std::string> paramTypes,
                                              choc::span<std::string> paramNames)
    {
        PaddedStringTable table;

        for (size_t i = 0; i < paramTypes.size(); ++i)
        {
            table.startRow();
            table.appendItem (i == 0 ? functionDecl : std::string());
            table.appendItem ((i == 0 ? "(" : " ") + paramTypes[i]);
            table.appendItem (paramNames[i] + (i < paramTypes.size() - 1 ? "," : ")"));
        }

        printTable (table, 1000);
    }
};

//==============================================================================
bool generateCode (choc::text::CodePrinter& printer, Program program, CompileMessageList& messageList, CodeGenOptions& options)
{
    try
    {
        CompileMessageHandler handler (messageList);

        CPPGenerator gen (printer, program, options);
        return gen.run();
    }
    catch (const AbortCompilationException&) {}

    return false;
}

std::string generateCode (Program program, CompileMessageList& messageList, CodeGenOptions& options)
{
    choc::text::CodePrinter printer;

    if (generateCode (printer, std::move (program), messageList, options))
        return printer.toString();

    return {};
}

} // namespace soul::cpp
