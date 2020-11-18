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
struct heart::Printer
{
    static void print (const Program& p, choc::text::CodePrinter& out)
    {
        out << '#' << getHEARTFormatVersionPrefix() << ' ' << getHEARTFormatVersion() << blankLine;

        for (auto& module : p.getModules())
            PrinterStream (module, out).printAll();
    }

    static std::string getDump (const Program& p)
    {
        choc::text::CodePrinter out;
        print (p, out);
        return out.toString();
    }

private:
    static constexpr choc::text::CodePrinter::NewLine newLine = {};
    static constexpr choc::text::CodePrinter::BlankLine blankLine = {};
    static constexpr choc::text::CodePrinter::SectionBreak sectionBreak = {};

    struct PrinterStream
    {
        PrinterStream (const Module& m, choc::text::CodePrinter& o)
           : module (m), out (o) {}

        const Module& module;
        choc::text::CodePrinter& out;

        std::unordered_map<pool_ref<heart::Variable>, std::string> localVariableNames;
        std::vector<std::string> allVisibleVariables;

        void printAll()
        {
            for (auto& v : module.stateVariables.get())
                allVisibleVariables.push_back (v->name);

            if (module.isProcessor())   out << "processor ";
            if (module.isGraph())       out << "graph ";
            if (module.isNamespace())   out << "namespace ";

            out << module.fullName;
            printDescription (module.annotation);
            out << newLine;

            {
                auto indent = out.createIndentWithBraces (2);

                if (! module.isNamespace())
                {
                    printInputs();
                    printOutputs();
                    out << blankLine;
                }

                printNodes();
                out << blankLine;
                printConnections();
                out << blankLine;
                printLatency();
                printStateVariables();
                out << blankLine;
                printStructs();
                out << blankLine;
                printFunctions();
            }

            out << blankLine;
        }

        void printDescription (const std::vector<Type>& types)
        {
            if (types.size() == 1)
            {
                out << getTypeDescription (types[0]);
                return;
            }

            out << '(';
            bool first = true;

            for (auto& type : types)
            {
                if (! first)
                    out << ", ";

                out << getTypeDescription (type);
                first = false;
            }

            out << ')';
        }

        void printDescription (const Annotation& annotation)
        {
            out << annotation.toHEART();
        }

        static std::string nameWithArray (const soul::Identifier& name, const std::optional<uint32_t>& arraySize)
        {
            if (arraySize.has_value())
                return name.toString() + '[' + std::to_string (*arraySize) + ']';

            return name.toString();
        }

        void printInputs()
        {
            for (auto io : module.inputs)
            {
                out << "input   "
                    << padded (nameWithArray (io->name, io->arraySize), 20)
                    << padded (getEndpointTypeName (io->endpointType), 8);

                printDescription (io->dataTypes);
                printDescription (io->annotation);
                out << ';' << newLine;
            }
        }

        void printOutputs()
        {
            for (auto io : module.outputs)
            {
                out << "output  "
                    << padded (nameWithArray (io->name, io->arraySize), 20)
                    << padded (getEndpointTypeName (io->endpointType), 8);

                printDescription (io->dataTypes);
                printDescription (io->annotation);
                out << ';' << newLine;
            }
        }

        void printNodes()
        {
            for (auto mi : module.processorInstances)
            {
                out << "node " << padded (mi->instanceName, 16) << " = " << mi->sourceName;

                if (mi->arraySize > 1)
                    out << '[' << mi->arraySize << ']';

                if (mi->clockMultiplier.hasValue())
                    out << " " << mi->clockMultiplier.toString();

                out << ';' << newLine;
            }
        }

        void printConnections()
        {
            for (auto c : module.connections)
            {
                out << "connection "
                    << getInterpolationDescription (c->interpolationType)
                    << ' ';

                printEndpointReference (c->source);

                if (c->delayLength)
                    out << " -> [" << *c->delayLength << ']';

                out << " -> ";
                printEndpointReference (c->dest);
                out << ';' << newLine;
            }
        }

        void printEndpointReference (const heart::EndpointReference& e)
        {
            if (e.processor != nullptr)
                out << e.processor->instanceName << ".";

            out << e.endpointName;

            if (e.endpointIndex)
                out << "[" << *e.endpointIndex << "]";
        }

        void printLatency()
        {
            if (module.latency != 0)
                out << "processor.latency = " << module.latency << ";" << newLine;
        }

        void printStateVariables()
        {
            heart::Utilities::VariableListByType list (module.stateVariables.get());

            for (auto& type : list.types)
            {
                for (auto& v : type.variables)
                {
                    out << (v->isExternal() ? "let external " : (v->isConstant() ? "let " : "var "))
                        << getTypeDescription (type.type) << ' ';

                    printVarWithPrefix (v->name);

                    if (v->initialValue != nullptr)
                    {
                        out << " = ";
                        printExpression (*v->initialValue);
                    }

                    printDescription (v->annotation);
                    out << ';' << newLine;
                }
            }
        }

        void printStructs()
        {
            for (auto& s : module.structs.get())
                printStruct (*s);
        }

        void printFunctions()
        {
            for (auto f : module.functions.get())
                printFunction (f);
        }

        void printParameters (ArrayView<pool_ref<Variable>> parameters)
        {
            if (parameters.empty())
            {
                out << "()";
            }
            else
            {
                out << " (";

                bool isFirst = true;

                for (auto p : parameters)
                {
                    if (isFirst)
                        isFirst = false;
                    else
                        out << ", ";

                    out << getTypeDescription (p->type) << ' ';
                    printVarWithPrefix (p->name);
                }

                out << ')';
            }
        }

        void printBlock (heart::Block& b)
        {
            auto labelIndent = out.createIndent (2);
            out << getBlockName (b);

            if (!b.parameters.empty())
                printParameters (b.parameters);

            out << ":" << newLine;

            auto statementIndent = out.createIndent (2);

            for (auto s : b.statements)
            {
                printStatementDescription (*s);
                out << ';' << newLine;
            }

            printStatementDescription (*b.terminator);
            out << ';' << newLine;
        }

        void printFunction (heart::Function& f)
        {
            SOUL_ASSERT (f.name.isValid());

            out << (f.functionType.isEvent() ? "event " : "function ");
            out << getFunctionName (f);

            printParameters (f.parameters);

            if (! f.functionType.isEvent())
                out << " -> " << getTypeDescription (f.returnType);

            printDescription (f.annotation);

            if (f.hasNoBody)
            {
                out << ';' << blankLine;
                return;
            }

            out << newLine
                << '{' << newLine;

            buildLocalVariableList (f);

            for (auto b : f.blocks)
                printBlock (b);

            out << '}' << blankLine;
        }

        void buildLocalVariableList (const heart::Function& f)
        {
            int unnamedVariableIndex = 0;
            localVariableNames.clear();
            auto localVars = f.getAllLocalVariables();
            std::vector<std::string> usedNames;
            usedNames.reserve (localVars.size());

            for (auto& v : f.parameters)
                usedNames.push_back (v->name.toString());

            for (auto& v : localVars)
            {
                SOUL_ASSERT (v->isMutableLocal() || v->isConstant());
                std::string name;

                if (v->name.isValid())
                {
                    name = v->name.toString();
                    SOUL_ASSERT (! name.empty());
                    name = addSuffixToMakeUnique (name, [&] (const std::string& nm) { return contains (usedNames, nm)
                                                                                              || contains (allVisibleVariables, nm); });
                    usedNames.push_back (name);
                }
                else
                {
                    name = std::to_string (unnamedVariableIndex++);
                }

                localVariableNames[v] = name;
            }
        }

        void printStruct (const Structure& s) const
        {
            out << "struct " << s.getName() << newLine;

            int maxTypeLen = 0;

            for (auto& m : s.getMembers())
                maxTypeLen = std::max (maxTypeLen, (int) getTypeDescription (m.type).length());

            {
                auto indent = out.createIndentWithBraces (2);

                for (auto& m : s.getMembers())
                    out << padded (getTypeDescription (m.type), maxTypeLen + 2) << m.name << ';' << newLine;
            }

            out << blankLine;
        }

        void printValue (const Value& v)
        {
            struct Printer  : public ValuePrinter
            {
                Printer (choc::text::CodePrinter& s) : outStream (s) {}

                void print (std::string_view s) override    { outStream << s; }
                void printFloat32 (float value) override    { if (value == 0) print ("0.0f"); else ValuePrinter::printFloat32 (value); }
                void printFloat64 (double value) override   { if (value == 0) print ("0.0");  else ValuePrinter::printFloat64 (value); }

                void printStringLiteral (StringDictionary::Handle h) override
                {
                    print (dictionary != nullptr ? toHeartStringLiteral (dictionary->getStringForHandle (h))
                                                 : std::to_string (h.handle));
                }

                choc::text::CodePrinter& outStream;
            };

            Printer p { out };

            if (! (v.getType().isPrimitiveInteger() || v.getType().isPrimitiveFloat()))
            {
                out << getTypeDescription (v.getType()) << ' ';
                p.dictionary = std::addressof (module.program.getStringDictionary());
            }

            v.print (p);
        }

        void printExpression (heart::Expression& e)
        {
            auto constValue = e.getAsConstant();

            if (constValue.isValid())
                return printValue (constValue);

            if (auto v = cast<heart::Variable> (e))
            {
                if (v->isMutableLocal() || v->isConstant())
                {
                    auto nm = localVariableNames.find (*v);

                    if (nm != localVariableNames.end())
                        return printVarWithPrefix (nm->second);

                    SOUL_ASSERT_FALSE;
                }

                return printVarWithPrefix (module.program.getVariableNameWithQualificationIfNeeded (module, *v));
            }

            if (auto arrayElement = cast<heart::ArrayElement> (e))
            {
                printExpression (arrayElement->parent);

                if (arrayElement->dynamicIndex != nullptr)
                {
                    out << '[';
                    printExpression (*arrayElement->dynamicIndex);
                    out << ']';
                    return;
                }

                if (arrayElement->isSingleElement())
                {
                    out << '[' << arrayElement->fixedStartIndex << ']';
                    return;
                }

                out << '[' << arrayElement->fixedStartIndex << ":" << arrayElement->fixedEndIndex << ']';
                return;
            }

            if (auto structElement = cast<heart::StructElement> (e))
            {
                printExpression (structElement->parent);
                out << "." << structElement->memberName;
                return;
            }

            if (auto c = cast<heart::TypeCast> (e))
            {
                out << "cast " << getTypeDescription (c->destType) << " (";
                printExpression (c->source);
                out << ')';
                return;
            }

            if (auto u = cast<heart::UnaryOperator> (e))
            {
                out << getUnaryOpName (u->operation) << " (";
                printExpression (u->source);
                out << ')';
                return;
            }

            if (auto b = cast<heart::BinaryOperator> (e))
            {
                out << getBinaryOpName (b->operation) << " (";
                printExpression (b->lhs);
                out << ", ";
                printExpression (b->rhs);
                out << ')';
                return;
            }

            if (auto fc = cast<heart::PureFunctionCall> (e))
            {
                out << getFunctionName (fc->function);
                printArgList (fc->arguments);
                return;
            }

            if (auto pp = cast<heart::ProcessorProperty> (e))
            {
                out << "processor." << pp->getPropertyName();
                return;
            }

            if (auto list = cast<heart::AggregateInitialiserList> (e))
            {
                printArgList (list->items);
                return;
            }

            SOUL_ASSERT_FALSE;
        }

        void printVarWithPrefix (const std::string& name)
        {
            SOUL_ASSERT (! name.empty());

            if (name[0] == '$')
                out << name;
            else
                out << "$" << removeCharacter (name, '$');
        }

        std::string getTypeDescription (const Type& type) const
        {
            return module.program.getTypeDescriptionWithQualificationIfNeeded (module, type.removeConstIfPresent());
        }

        static const char* getUnaryOpName (UnaryOp::Op o)
        {
            #define SOUL_GET_UNARY_OP_NAME(name, op)   if (o == UnaryOp::Op::name) return #name;
            SOUL_UNARY_OPS (SOUL_GET_UNARY_OP_NAME)
            #undef SOUL_GET_UNARY_OP_NAME
            SOUL_ASSERT_FALSE;
            return "";
        }

        static const char* getBinaryOpName (BinaryOp::Op o)
        {
            #define SOUL_GET_BINARY_OP_NAME(name, op)   if (o == BinaryOp::Op::name) return #name;
            SOUL_BINARY_OPS (SOUL_GET_BINARY_OP_NAME)
            #undef SOUL_GET_BINARY_OP_NAME
            SOUL_ASSERT_FALSE;
            return "";
        }

        const char* getAssignmentRole (heart::Expression& e)
        {
            if (auto v = cast<heart::Variable> (e))
                if (v->isConstant())
                    return "let ";

            return "";
        }

        void printAssignmentSyntax (heart::Expression& e)
        {
            out << getAssignmentRole (e);
            printExpression (e);
            out << " = ";
        }

        std::string getFunctionName (const heart::Function& f)   { return module.program.getFunctionNameWithQualificationIfNeeded (module, f); }
        static std::string getBlockName (heart::Block& b)        { return b.name; }

        void printStatementDescription (const heart::Object& s)
        {
            #define SOUL_GET_STATEMENT_DESC(Type)     if (auto t = cast<const heart::Type> (s)) return printDescription (*t);
            SOUL_HEART_STATEMENTS (SOUL_GET_STATEMENT_DESC)
            SOUL_HEART_TERMINATORS (SOUL_GET_STATEMENT_DESC)

            SOUL_ASSERT_FALSE;
        }

        void printDescription (const heart::Branch& b)
        {
            out << "branch " << getBlockName (b.target);

            if (! b.targetArgs.empty())
                printArgList (b.targetArgs);
        }

        void printDescription (const heart::BranchIf& b)
        {
            out << "branch_if ";
            printExpression (b.condition);
            out << " ? " << getBlockName (b.targets[0]);

            if (! b.targetArgs[0].empty())
                printArgList (b.targetArgs[0]);

            out << " : " << getBlockName (b.targets[1]);

            if (! b.targetArgs[1].empty())
                printArgList (b.targetArgs[1]);
        }

        void printDescription (const heart::ReturnVoid&) const
        {
            out << "return";
        }

        void printDescription (const heart::ReturnValue& r)
        {
            out << "return ";
            printExpression (r.returnValue);
        }

        void printDescription (const heart::AssignFromValue& a)
        {
            printAssignmentSyntax (*a.target);
            printExpression (a.source);
        }

        void printArgList (const ArrayView<pool_ref<heart::Expression>>& args)
        {
            if (args.empty())
            {
                out << "()";
                return;
            }

            out << " (";
            bool isFirst = true;

            for (auto& arg : args)
            {
                if (isFirst)
                    isFirst = false;
                else
                    out << ", ";

                printExpression (arg);
            }

            out << ')';
        }

        void printDescription (const heart::FunctionCall& f)
        {
            if (f.target != nullptr)
                printAssignmentSyntax (*f.target);

            out << "call " << getFunctionName (f.getFunction());
            printArgList (f.arguments);
        }

        void printDescription (const heart::ReadStream& r)
        {
            out << getAssignmentRole (*r.target);
            printExpression (*r.target);
            out << " = read " << r.source->name.toString();
        }

        void printDescription (const heart::WriteStream& w)
        {
            out << "write " << w.target->name.toString();

            if (w.element != nullptr)
            {
                out << '[';
                printExpression (*w.element);
                out << ']';
            }

            out << ' ';
            printExpression (w.value);
        }

        void printDescription (const heart::AdvanceClock&) const
        {
            out << "advance";
        }
    };
};

} // namespace soul
