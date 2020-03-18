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


namespace soul::patch
{
    class BelaWrapper
    {
    public:

        static std::string build (const soul::Program& program)
        {
            BelaWrapper wrapper (program);

            return wrapper.buildWrapper();
        }

    private:
        BelaWrapper (const soul::Program& program_) : program (program_)
        {
        }

        std::string buildWrapper()
        {
            auto mainProcessor = program.getMainProcessor();

            parameters << "wrappedModule = " << mainProcessor->getNameWithoutRootNamespace() << ";" << newLine;

            uint32_t nextOutputChannel = 0;

            const bool useBelaParameters = useBelaParameterNumbers();

            for (auto& input : mainProcessor->inputs)
            {
                auto& name = input->name.toString();
                auto minValue = input->annotation.getDouble ("min", 0.0);
                auto maxValue = input->annotation.getDouble ("max", 1.0);

                if (isParameterEvent (input))
                {
                    addInputParameter (input, "Bela::InputParameterEvent", name, getParameterId (input, useBelaParameters), minValue, maxValue);
                }
                else if (isParameterStream (input))
                {
                    addInputParameter (input, "Bela::InputParameterStream", name, getParameterId (input, useBelaParameters), minValue, maxValue);
                }
                else if (isAudioStream (input))
                {
                    auto channels = input->dataTypes.front().getVectorSize();

                    parameters << name << "Input = Bela::InputAudioStream (" << std::to_string (nextOutputChannel) << ", " << channels << ");" << newLine;
                    connections << "audioIn -> " << name << "Input.audioIn;" << newLine;
                    connections << name << "Input.audioOut -> wrappedModule." << name << ";" << newLine;
                    connections << newLine;

                    nextOutputChannel += static_cast<uint32_t> (channels);
                }
                else
                {
                    streams << "input " << getEndpointKindName (input->kind) << " " << getSampleTypeString (input)
                            << " " << name << input->annotation.toHEART (program.getStringDictionary()) << ";" << newLine;

                    connections << name << " -> " << "wrappedModule." << name << ";" << newLine;
                }
            }

            for (auto& output : mainProcessor->outputs)
            {
                auto& name      = output->name.toString();
                auto typeString = getSampleTypeString (output);

                streams << "output " << getEndpointKindName (output->kind) <<" " << typeString << " " << name << ";" << newLine;
                connections << "wrappedModule." << name << " -> " << name << ";" << newLine;
            }

            IndentedStream graph;

            graph.writeMultipleLines (namespaceCode);

            graph << blankLine
                  << "graph BelaWrapper [[ main ]]" << newLine;

            {
                auto indent1 = graph.createBracedIndent();

                graph << "input stream float<10> audioIn;" << newLine;
                graph.writeMultipleLines (streams.toString());
                graph << blankLine
                      << "let" << newLine;

                {
                    auto indent2 = graph.createBracedIndent();
                    graph.writeMultipleLines (parameters.toString());
                }

                graph << blankLine
                      << "connection" << newLine;

                {
                    auto indent2 = graph.createBracedIndent();
                    graph.writeMultipleLines (connections.toString());
                }

                graph << newLine;
            }

            graph << newLine;

            return graph.toString();
        }

        int getParameterId (const soul::heart::InputDeclaration& input, bool useBelaParameters)
        {
            if (! useBelaParameters)
                return (nextParameterId < maxParameters) ? nextParameterId++ :  -1;

            return static_cast<int> (input.annotation.getInt64 ("belaControl", -1));
        }

        bool isParameterAnnotation (const soul::Annotation& annotation)
        {
            return annotation.hasValue ("name")
                || annotation.hasValue ("min")
                || annotation.hasValue ("max");
        }

        bool isParameterEvent (const soul::heart::InputDeclaration& input)
        {
            return input.isEventEndpoint() && isParameterAnnotation (input.annotation);
        }

        bool isParameterStream (const soul::heart::InputDeclaration& input)
        {
            return input.isStreamEndpoint() && isParameterAnnotation (input.annotation);
        }

        bool isAudioStream (const soul::heart::InputDeclaration& input)
        {
            return input.isStreamEndpoint();
        }

        std::string getSampleTypeString (const soul::heart::InputDeclaration& input)
        {
            auto type = input.getSingleSampleType();

            if (type.isStruct())
                return program.getStructNameWithQualificationIfNeeded (*program.getMainProcessor(), *type.getStruct());

            return type.getDescription();
        }

        std::string getSampleTypeString (const soul::heart::OutputDeclaration& output)
        {
            return output.getSingleSampleType().getDescription();
        }

        bool useBelaParameterNumbers()
        {
            auto mainProcessor = program.getMainProcessor();

            for (auto& input : mainProcessor->inputs)
                if (input->annotation.hasValue ("belaControl"))
                    return true;

            return false;
        }

        void addInputParameter (const soul::heart::InputDeclaration& input, const std::string& type, const std::string& name, int parameterId, double minValue, double maxValue)
        {
            if (parameterId >= 0)
            {
                parameters << name << "Param = " << type << " (" << std::to_string (parameterChannelOffset + parameterId) << ", float(" << minValue << "), float (" << maxValue << "));" << newLine;
                connections << "audioIn -> " << name << "Param.audioIn;" << newLine;
                connections << name << "Param.out -> wrappedModule." << name << ";" << newLine;
                connections << newLine;
            }
            else
            {
                streams << "input " << getEndpointKindName (input.kind) << " " << getSampleTypeString (input) << " " << name << input.annotation.toHEART(program.getStringDictionary()) << ";" << newLine;
                connections << name << " -> " << "wrappedModule." << name << ";" << newLine;
            }
        }

        const soul::Program& program;

        IndentedStream parameters, connections, streams;

        int  nextParameterId              = 0;
        const int maxParameters           = 8;
        const int  parameterChannelOffset = 2;


        const std::string namespaceCode = R"(
namespace Bela
{
    let resolution = 16;

//    let inputMaxValue = 0.34f;      // When powered from USB
    let inputMaxValue = 1.0f;       // When powered from 12v (Eurorack)

    processor InputParameterStream (int channel, float min, float max)
    {
        input stream float<10> audioIn;
        output stream float out;

        void run()
        {
            loop
            {
                let i = audioIn[channel] / inputMaxValue;
                let scaledValue = min + (max - min) * i;
                let truncatedValue = min (max, max (min, scaledValue));

                loop (resolution)
                {
                    out << truncatedValue;
                    advance();
                }
            }
        }
    }

    processor InputParameterEvent (int channel, float min, float max)
    {
        input stream float<10> audioIn;
        output event float out;

        void process (float v)
        {
            let i = v / inputMaxValue;
            let scaledValue = min + (max - min) * i;
            let truncatedValue = min (max, max (min, scaledValue));

            out << truncatedValue;
        }

        void run()
        {
            loop
            {
                process (audioIn[channel]);

                loop (resolution)
                {
                    advance();
                }
            }

            advance();
        }
    }

    processor InputAudioStream (int startChannel, int channelCount)
    {
        input stream float<10> audioIn;
        output stream float<channelCount> audioOut;

        void run()
        {
            loop
            {
                audioOut << float<channelCount> (audioIn [startChannel: startChannel + channelCount]);
                advance();
            }
        }
    }
}

)";
    };
}
