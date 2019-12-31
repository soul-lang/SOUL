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
    static void print (const Program& p, IndentedStream& out)
    {
        out << "#" << getHEARTFormatVersionPrefix()
            << " " << std::to_string (getHEARTFormatVersion()) << blankLine;

        for (auto& module : p.getModules())
            PrinterStream (p, *module, out).printAll();
    }

    static std::string getDump (const Program& p)
    {
        IndentedStream out;
        print (p, out);
        return out.toString();
    }

private:
    struct PrinterStream
    {
        PrinterStream (const Program& p, const Module& m, IndentedStream& o)
           : program (p), module (m), out (o) {}

        const Program& program;
        const Module& module;
        IndentedStream& out;

        std::unordered_map<heart::VariablePtr, std::string> localVariableNames;
        std::vector<std::string> allVisibleVariables;

        void printAll()
        {
            for (auto& v : module.stateVariables)
                allVisibleVariables.push_back (v->name);

            if (module.isProcessor())   out << "processor ";
            if (module.isGraph())       out << "graph ";
            if (module.isNamespace())   out << "namespace ";

            out << module.moduleName << getDescription (module.annotation) << newLine;

            {
                auto indent = out.createBracedIndent (2);

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
                printStateVariables();
                out << blankLine;
                printStructs();
                out << blankLine;
                printFunctions();
            }

            out << blankLine;
        }

        std::string getDescription (const std::vector<Type>& types)
        {
            if (types.size() == 1)
                return getTypeDescription (types[0]);

            std::ostringstream oss;
            oss << "(";

            bool first = true;

            for (auto& type : types)
            {
                if (! first)
                    oss << ", ";

                oss << getTypeDescription (type);
                first = false;
            }

            oss << ")";
            return oss.str();
        }

        std::string getDescription (const Annotation& annotation)
        {
            return annotation.toHEART (program.getStringDictionary());
        }

        std::string nameWithArray (const soul::Identifier& name, uint32_t arraySize)
        {
            if (arraySize > 1)
                return name.toString() + "[" + std::to_string (arraySize) + "]";

            return name.toString();
        }

        void printInputs()
        {
            for (auto io : module.inputs)
                out << "input   "
                    << padded (nameWithArray (io->name, io->arraySize), 20)
                    << padded (getEndpointKindName (io->kind), 8)
                    << getDescription (io->sampleTypes)
                    << getDescription (io->annotation) << ";" << newLine;
        }

        void printOutputs()
        {
            for (auto io : module.outputs)
                out << "output  "
                    << padded (nameWithArray (io->name, io->arraySize), 20)
                    << padded (getEndpointKindName (io->kind), 8)
                    << getDescription (io->sampleTypes)
                    << getDescription (io->annotation) << ";" << newLine;
        }

        void printNodes()
        {
            for (auto mi : module.processorInstances)
            {
                out << "node " << padded (mi->instanceName, 16) << " = " << mi->sourceName;

                if (mi->arraySize > 1)
                    out << "[" << std::to_string (mi->arraySize) << "]";

                if (! mi->specialisationArgs.empty())
                {
                    out << "(";
                    bool first = true;

                    for (auto& a : mi->specialisationArgs)
                    {
                        if (first)
                            first = false;
                        else
                            out << ", ";

                        out << a.toString();
                    }

                    out << ")";
                }

                // Can't have both
                SOUL_ASSERT (! (mi->hasClockMultiplier() && mi->hasClockDivider()));

                if (mi->hasClockMultiplier())   out << " * " << std::to_string (mi->clockMultiplier);
                if (mi->hasClockDivider())      out << " / " << std::to_string (mi->clockDivider);

                out << ";" << newLine;
            }
        }

        void printConnections()
        {
            for (auto c : module.connections)
            {
                out << "connection "
                    << getInterpolationDescription (c->interpolationType)
                    << " "
                    << printProcessorAndChannel (c->sourceProcessor, c->sourceEndpoint);

                if (c->delayLength > 0)
                    out << " -> [" << std::to_string (c->delayLength) << "]";

                out << " -> "
                    << printProcessorAndChannel (c->destProcessor, c->destEndpoint)
                    << ";" << newLine;
            }
        }

        std::string printProcessorAndChannel (const heart::ProcessorInstancePtr m, const std::string& channel)
        {
            if (m == nullptr)
                return channel;

            return m->instanceName + "." + channel;
        }

        void printStateVariables()
        {
            heart::VariableListByType list (module.stateVariables);

            for (auto& type : list.types)
                for (auto& v : type.variables)
                    out << "var " << (v->isExternal() ? "external " : "") << getTypeDescription (type.type)
                        << " " << addVarPrefix (v->name.toString())
                        << getDescription (v->annotation) << ";" << newLine;
        }

        void printStructs()
        {
            for (auto& s : module.structs)
                printStruct (*s);
        }

        void printFunctions()
        {
            for (auto f : module.functions)
                printFunction (*f);
        }

        void printFunction (heart::Function& f)
        {
            SOUL_ASSERT (f.name.isValid());

            out << (f.functionType.isEvent() ? "event " : "function ");
            out << getFunctionName (f) << "(";

            {
                bool isFirst = true;

                for (auto p : f.parameters)
                {
                    if (isFirst)
                        isFirst = false;
                    else
                        out << ", ";

                    out << getTypeDescription (p->type) + " " + addVarPrefix (p->name.toString());
                }
            }

            out << ")";

            if (! f.functionType.isEvent())
                out << " -> " << getTypeDescription (f.returnType);

            out << getDescription (f.annotation);

            if (f.hasNoBody)
            {
                out << ";" << blankLine;
                return;
            }

            out << newLine
                << "{" << newLine;

            buildLocalVariableList (f);

            for (auto b : f.blocks)
            {
                auto labelIndent = out.createIndent (2);
                out << getBlockName (b) << ":" << newLine;

                auto statementIndent = out.createIndent (2);

                for (auto s : b->statements)
                    out << getStatementDescription (*s) << ";" << newLine;

                out << getStatementDescription (b->terminator) << ";" << newLine;
            }

            out << "}" << blankLine;
        }

        void buildLocalVariableList (const heart::Function& f)
        {
            int unnamedVariableIndex = 0;
            localVariableNames.clear();
            auto localVars = f.getAllLocalVariables();
            std::vector<std::string> usedNames;

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

                localVariableNames[v] = addVarPrefix (name);
            }
        }

        void printStruct (const Structure& s) const
        {
            out << "struct " << s.name << newLine;

            int maxTypeLen = 0;

            for (auto& m : s.members)
                maxTypeLen = std::max (maxTypeLen, (int) getTypeDescription (m.type).length());

            {
                auto indent = out.createBracedIndent (2);

                for (auto& m : s.members)
                    out << padded (getTypeDescription (m.type), maxTypeLen + 2) << m.name << ";" << newLine;
            }

            out << blankLine;
        }

        std::string getDesc (const Value& v)
        {
            if (v.getType().isPrimitiveInteger())
                return v.getDescription();

            if (v.getType().isStringLiteral())
                return getTypeDescription (v.getType()) + " "
                         + addDoubleQuotes (program.getStringDictionary().getStringForHandle (v.getStringLiteral()));

            if (v.getType().isPrimitiveFloat())
            {
                if (v.isZero())
                    return v.getType().isFloat32() ? "0.0f" : "0.0";

                return v.getDescription();
            }

            return getTypeDescription (v.getType()) + " "  + v.getDescription();
        }

        std::string getDesc (heart::Expression& e)
        {
            auto constValue = e.getAsConstant();

            if (constValue.isValid())
                return getDesc (constValue);

            if (auto v = cast<heart::Variable> (e))
            {
                if (v->isMutableLocal() || v->isConstant())
                {
                    auto nm = localVariableNames.find (v);

                    if (nm != localVariableNames.end())
                    {
                        SOUL_ASSERT (! nm->second.empty());
                        SOUL_ASSERT (nm->second[0] == '$');
                        return nm->second;
                    }

                    SOUL_ASSERT (v->name.isValid());
                    return addVarPrefix (program.getVariableNameWithQualificationIfNeeded (module, *v));
                }

                SOUL_ASSERT (v->name.isValid());
                return addVarPrefix (v->name);
            }

            if (auto subElement = cast<heart::SubElement> (e))
            {
                auto desc = getDesc (*subElement->parent);

                if (subElement->dynamicIndex != nullptr)
                    return desc + "[" + getDesc (*subElement->dynamicIndex) + "]";

                if (subElement->parent->getType().isStruct())
                    return desc + "." + subElement->parent->getType().getStructRef().members[subElement->fixedStartIndex].name;

                if (subElement->isSingleElement())
                    return desc + "[" + std::to_string (subElement->fixedStartIndex) + "]";

                return desc + "[" + std::to_string (subElement->fixedStartIndex) + ":" + std::to_string (subElement->fixedEndIndex) + "]";
            }

            if (auto c = cast<heart::TypeCast> (e))
                return "cast " + getTypeDescription (c->destType) + " (" + getDesc (*c->source) + ")";

            if (auto u = cast<heart::UnaryOperator> (e))
                return getUnaryOpName (u->operation) + std::string (" (") + getDesc (*u->source) + ")";

            if (auto b = cast<heart::BinaryOperator> (e))
                return getBinaryOpName (b->operation) + std::string (" (") + getDesc (*b->lhs)
                          + ", " + getDesc (*b->rhs) + ")";

            if (auto fc = cast<heart::PureFunctionCall> (e))
                return getFunctionName (fc->function) + getArgListDescription (fc->arguments);

            if (auto fc = cast<heart::PlaceholderFunctionCall> (e))
                return fc->name + " " + getTypeDescription (fc->returnType) + getArgListDescription (fc->arguments);

            if (auto pp = cast<heart::ProcessorProperty> (e))
                return std::string ("processor.") + pp->getPropertyName();

            SOUL_ASSERT_FALSE;
            return {};
        }

        std::string addVarPrefix (std::string name)
        {
            return name[0] == '$' ? name : "$" + name;
        }

        std::string getTypeDescription (const Type& type) const
        {
            return program.getTypeDescriptionWithQualificationIfNeeded (module, type.removeConstIfPresent());
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

        const char* getAssignmentRole (heart::ExpressionPtr e)
        {
            if (auto v = cast<heart::Variable> (e))
                if (v->isConstant())
                    return "let ";

            return "";
        }

        std::string getAssignmentSyntax (heart::Expression& e)
        {
            return getAssignmentRole (e) + getDesc (e) + " = ";
        }

        std::string getFunctionName (const heart::Function& f)
        {
            return program.getFunctionNameWithQualificationIfNeeded (module, f);
        }

        static std::string getBlockName (heart::BlockPtr b)
        {
            return b->name;
        }

        std::string getStatementDescription (heart::ObjectPtr s)
        {
            #define SOUL_GET_STATEMENT_DESC(Type)     if (auto t = cast<const heart::Type> (s)) return getDescription (*t);
            SOUL_HEART_STATEMENTS (SOUL_GET_STATEMENT_DESC)
            SOUL_HEART_TERMINATORS (SOUL_GET_STATEMENT_DESC)

            SOUL_ASSERT_FALSE;
            return "<unknown>";
        }

        std::string getDescription (const heart::Branch& b) const
        {
            return "branch " + getBlockName (b.target);
        }

        std::string getDescription (const heart::BranchIf& b)
        {
            return "branch_if " + getDesc (*b.condition)
                     + " ? " + getBlockName (b.targets[0]) + " : " + getBlockName (b.targets[1]);
        }

        std::string getDescription (const heart::ReturnVoid&) const
        {
            return "return";
        }

        std::string getDescription (const heart::ReturnValue& r)
        {
            return "return " + getDesc (*r.returnValue);
        }

        std::string getDescription (const heart::AssignFromValue& a)
        {
            return getAssignmentSyntax (*a.target) + getDesc (*a.source);
        }

        std::string getArgListDescription (ArrayView<heart::ExpressionPtr> args)
        {
            if (args.empty())
                return "()";

            std::string desc (" (");
            bool isFirst = true;

            for (auto& arg : args)
            {
                if (isFirst)
                    isFirst = false;
                else
                    desc += ", ";

                desc += getDesc (*arg);
            }

            return desc + ")";
        }

        std::string getDescription (const heart::FunctionCall& f)
        {
            return (f.target == nullptr ? std::string() : getAssignmentSyntax (*f.target))
                     + "call " + getFunctionName (f.getFunction()) + getArgListDescription (f.arguments);
        }

        std::string getDescription (const heart::ReadStream& r)
        {
            return getAssignmentRole (r.target) + getDesc (*r.target) + " = read " + r.source->name.toString();
        }

        std::string getDescription (const heart::WriteStream& w)
        {
            if (w.element == nullptr)
                return "write " + w.target->name.toString() + " " + getDesc (*w.value);

            return "write " + w.target->name.toString() + "[" + getDesc (*w.element) + "]" + " " + getDesc (*w.value);
        }

        std::string getDescription (const heart::AdvanceClock&) const
        {
            return "advance";
        }
    };
};

} // namespace soul
