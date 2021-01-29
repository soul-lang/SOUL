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
    Converts complex primitives into an implementation using the soul::complex_lib namespace
    which contains a struct based complex number implementation.

    The transformation is performed in multiple passes. Binary and Unary operators are replaced by calls to
    associated methods in the namespace, and complex member references (e.g. real and imag) and mapped to the
    appropriate struct members. Finally instantiations of the complex_lib namespace are added for the given data
    type and vector size, and the complex types are replaced with the appropriate ComplexType structs

    A subsequent resolution pass is required to resolve the identifiers added by this process, and then any affected
    structs are regenerated to have the correct members.
*/
struct ConvertComplexPass  final
{
    static void run (AST::Allocator& a, AST::Namespace& m)
    {
        ConvertComplexPass (a, m).run();
    }

private:
    ConvertComplexPass (AST::Allocator& a, AST::Namespace& m) : allocator (a), module (m)
    {
    }

    AST::Allocator& allocator;
    AST::Namespace& module;

    struct Transformations
    {
        AST::Expression& getTransformedExpression (AST::Expression& original)
        {
            auto i = objects.find (&original);

            if (i != objects.end())
                return *cast<AST::Expression> (*i->second);

            return original;
        }

        AST::Statement& getTransformedStatement (AST::Statement& original)
        {
            auto i = objects.find (&original);

            if (i != objects.end())
                return *cast<AST::Statement> (*i->second);

            return original;
        }

        void addTransformation (AST::ASTObject& original, AST::ASTObject& replacement)
        {
            objects[&original] = &replacement;
        }

        std::unordered_map<AST::ASTObject*, AST::ASTObject*> objects;
    };

    Transformations transformations;

    static AST::UnqualifiedName& identifierFromString (AST::Allocator& allocator, AST::Context& context, const std::string& s)
    {
        return allocator.allocate<AST::UnqualifiedName> (context,  allocator.get (s));
    }

    static bool requiresRemapping (const soul::Type& type)
    {
        return type.isComplex();
    }

    static bool requiresRemapping (soul::pool_ptr<soul::AST::Expression> expr)
    {
        if (auto csl = cast<AST::CommaSeparatedList> (expr))
            return false;

        return requiresRemapping (expr->getResultType());
    }

    void run()
    {
        resetResolutionFlag (module);
        BuildTransformations (*this).visitObject (module);
        ApplyTransformations (transformations).visitObject (module);
        ConvertComplexRemapTypes (*this).run();
        ASTUtilities::removeModulesWithSpecialisationParams (module);
        ResolutionPass::run (allocator, module, false);
    }

    void resetResolutionFlag (AST::ModuleBase& m)
    {
        m.isFullyResolved = false;

        if (auto n = cast<AST::Namespace> (m))
            for (auto& childModule : n->getSubModules())
                resetResolutionFlag (childModule);
    }

    //==============================================================================
    struct ApplyTransformations : public RewritingASTVisitor
    {
        ApplyTransformations (Transformations& t) : transformations (t) {}

        using super = RewritingASTVisitor;

        Transformations& transformations;

        AST::Expression& visit (AST::UnaryOperator& u) override
        {
            super::visit (u);
            return transformations.getTransformedExpression (u);
        }

        AST::Expression& visit (AST::BinaryOperator& b) override
        {
            super::visit (b);
            return transformations.getTransformedExpression (b);
        }

        AST::Expression& visit (AST::ComplexMemberRef& s) override
        {
            super::visit (s);
            return transformations.getTransformedExpression (s);
        }

        AST::Statement& visit (AST::ReturnStatement& r) override
        {
            super::visit (r);
            return transformations.getTransformedStatement (r);
        }

        AST::Expression& visit (AST::TypeCast& t) override
        {
            super::visit (t);
            return transformations.getTransformedExpression (t);
        }

        AST::Expression& visit (AST::Assignment& a) override
        {
            super::visit (a);
            return transformations.getTransformedExpression (a);
        }

        AST::Expression& visit (AST::ArrayElementRef& r) override
        {
            super::visit (r);
            return transformations.getTransformedExpression (r);
        }
    };

    //==============================================================================
    struct BuildTransformations  : public ASTVisitor
    {
        BuildTransformations (ConvertComplexPass& rp) : allocator (rp.allocator), module (rp.module), transformations (rp.transformations) {}

        using super = ASTVisitor;
        static inline constexpr const char* getPassName()  { return "BuildTransformations"; }
        AST::Allocator& allocator;
        AST::ModuleBase& module;
        Transformations& transformations;

        void visit (AST::ComplexMemberRef& s) override
        {
            if (auto v = cast<AST::ArrayElementRef> (s.object))
            {
                if (! v->object->isResolved() || v->object->getResultType().isVector())
                {
                    // Convert a[b].c to a.c[b]
                    auto& memberRef = allocator.allocate<AST::DotOperator> (s.context, *v->object, identifierFromString (allocator, s.context, s.memberName));

                    transformations.addTransformation (s, allocator.allocate<AST::ArrayElementRef> (s.context, memberRef, v->startIndex, v->endIndex, v->isSlice));
                    return;
                }
            }
            else
            {
                super::visit (s);
            }

            // Convert back to a dot operator, so that the subsequent resolution pass will convert it to the right struct member access
            transformations.addTransformation (s, allocator.allocate<AST::DotOperator> (s.context, s.object, identifierFromString (allocator, s.context, s.memberName)));
        }

        void visit (AST::UnaryOperator& u) override
        {
            super::visit (u);

            if (u.isResolved())
            {
                if (requiresRemapping (u.getResultType()))
                {
                    // Convert to a function call
                    auto functionName = getFunctionNameForOperator (u);
                    auto& args = allocator.allocate<AST::CommaSeparatedList> (u.context);
                    args.items.push_back (u.source);

                    transformations.addTransformation (u, allocator.allocate<AST::CallOrCast> (functionName, args, true));
                }
            }
        }

        void visit (AST::BinaryOperator& b) override
        {
            super::visit (b);

            if (b.isResolved())
            {
                if (requiresRemapping (b.getOperandType()))
                {
                    // Convert to a function call
                    auto functionName = getFunctionNameForOperator (b);
                    auto& args = allocator.allocate<AST::CommaSeparatedList> (b.context);
                    args.items.push_back (addCastIfRequired (b.lhs, b.getOperandType()));
                    args.items.push_back (addCastIfRequired (b.rhs, b.getOperandType()));

                    transformations.addTransformation (b, allocator.allocate<AST::CallOrCast> (functionName, args, true));
                }
            }
        }

        void visit (AST::ReturnStatement& r) override
        {
            super::visit (r);

            auto returnTypeExp = r.getParentFunction()->returnType;

            if (AST::isResolvedAsType (returnTypeExp)
                 && requiresRemapping (returnTypeExp->resolveAsType())
                 && r.returnValue->isResolved())
            {
                auto& returnStatement = allocator.allocate<AST::ReturnStatement> (r.context);
                returnStatement.returnValue = addCastIfRequired (*r.returnValue, returnTypeExp->resolveAsType());

                transformations.addTransformation (r, returnStatement);
            }
        }

        void visit (AST::TypeCast& t) override
        {
            super::visit (t);

            if (requiresRemapping (t.targetType))
                if (t.source->isResolved() && requiresRemapping (t.source))
                    transformations.addTransformation (t, addCastIfRequired (t.source, t.targetType));
        }

        void visit (AST::Assignment& a) override
        {
            if (a.isResolved() && requiresRemapping (a.getResultType()))
            {
                a.newValue = addCastIfRequired (a.newValue, a.getResultType());

                if (auto v = cast<AST::ArrayElementRef> (a.target))
                {
                    if (v->object->getResultType().isVector())
                    {
                        super::visitObject (a.newValue);

                        auto& functionName = allocator.allocate<AST::QualifiedIdentifier> (a.context, soul::IdentifierPath::fromString (allocator.identifiers, "setElement"));
                        auto& args = allocator.allocate<AST::CommaSeparatedList> (a.context);
                        args.items.push_back (*v->object);
                        args.items.push_back (*v->startIndex);
                        args.items.push_back (addCastIfRequired (a.newValue, v->object->getResultType().getVectorElementType()));

                        auto& call = allocator.allocate<AST::CallOrCast> (functionName, args, true);

                        transformations.addTransformation (a, call);
                        return;
                    }
                }
            }

            super::visit (a);
        }

        void visit (AST::ArrayElementRef& r) override
        {
            super::visit (r);

            if (r.isResolved() && requiresRemapping (r.getResultType()) && r.object->getResultType().isVector())
            {
                // Convert this to a method call
                auto& functionName = allocator.allocate<AST::QualifiedIdentifier> (r.context, soul::IdentifierPath::fromString (allocator.identifiers, "getElement"));
                auto& args = allocator.allocate<AST::CommaSeparatedList> (r.context);
                args.items.push_back (*r.object);
                args.items.push_back (*r.startIndex);

                transformations.addTransformation (r, allocator.allocate<AST::CallOrCast> (functionName, args, true));
            }
        }

    private:
        AST::Expression& addCastIfRequired (AST::Expression& e, const Type& targetType)
        {
            SOUL_ASSERT (targetType.isComplex());

            auto& transformedExpression = transformations.getTransformedExpression (e);

            auto sourceType = e.getResultType();

            if (sourceType.isEqual (targetType, Type::ComparisonFlags::ignoreConst | Type::ComparisonFlags::ignoreReferences))
                return transformedExpression;

            if (sourceType.isComplex())
            {
                auto memberType = targetType.isComplex32() ? PrimitiveType::float32 : PrimitiveType::float64;

                // Cast the real/imaginary components to the targetType
                auto& args = allocator.allocate<AST::CommaSeparatedList> (e.context);

                auto& real = allocator.allocate<AST::DotOperator> (e.context, transformedExpression, identifierFromString (allocator, e.context, "real"));
                args.items.push_back (allocator.allocate<AST::TypeCast> (e.context, memberType, real));

                auto& imag = allocator.allocate<AST::DotOperator> (e.context, transformedExpression, identifierFromString (allocator, e.context, "imag"));
                args.items.push_back (allocator.allocate<AST::TypeCast> (e.context, memberType, imag));

                return allocator.allocate<AST::TypeCast> (e.context, targetType.removeReferenceIfPresent(), args);
            }

            return allocator.allocate<AST::TypeCast> (e.context, targetType.removeReferenceIfPresent(), transformedExpression);
        }

        pool_ref<AST::QualifiedIdentifier> getFunctionNameForOperator (AST::UnaryOperator& u)
        {
            std::string functionName;

            switch (u.operation)
            {
                case UnaryOp::Op::negate:       functionName = "negate"; break;

                default:
                    u.context.throwError (soul::Errors::wrongTypeForUnary());
            }

            return allocator.allocate<AST::QualifiedIdentifier> (u.context, soul::IdentifierPath::fromString (allocator.identifiers, functionName));
        }

        pool_ref<AST::QualifiedIdentifier> getFunctionNameForOperator (AST::BinaryOperator& b)
        {
            std::string functionName;

            switch (b.operation)
            {
                case BinaryOp::Op::add:         functionName = "add";        break;
                case BinaryOp::Op::subtract:    functionName = "subtract";   break;
                case BinaryOp::Op::multiply:    functionName = "multiply";   break;
                case BinaryOp::Op::divide:      functionName = "divide";     break;
                case BinaryOp::Op::equals:      functionName = "equals";     break;
                case BinaryOp::Op::notEquals:   functionName = "notEquals";  break;

                default:
                    b.context.throwError (soul::Errors::illegalTypesForBinaryOperator (BinaryOp::getSymbol (b.operation),
                                                                                       b.lhs->getResultType().getDescription(),
                                                                                       b.rhs->getResultType().getDescription()));
            }

            return allocator.allocate<AST::QualifiedIdentifier> (b.context, soul::IdentifierPath::fromString (allocator.identifiers, functionName));
        }
    };

    //==============================================================================
    struct ConvertComplexRemapTypes  : public RewritingASTVisitor
    {
        ConvertComplexRemapTypes (ConvertComplexPass& p)  : allocator (p.allocator), module (p.module)
        {
            complexLib = getModule (soul::IdentifierPath::fromString (allocator.identifiers, "soul::complex_lib"));
            SOUL_ASSERT (complexLib != nullptr);
        }

        void run()
        {
            visitObject (module);
            ResolutionPass::run (allocator, module, true);
        }

        using super = RewritingASTVisitor;
        static inline constexpr const char* getPassName()  { return "ConvertComplexRemapTypes"; }

        AST::Allocator& allocator;
        AST::ModuleBase& module;
        pool_ptr<AST::ModuleBase> complexLib;

        AST::Expression& visit (AST::ConcreteType& t) override
        {
            super::visit (t);

            if (requiresRemapping (t.type))
                return getRemappedType (t.context, t.type);

            return t;
        }

        AST::StructDeclaration& visit (AST::StructDeclaration& s) override
        {
            auto r = itemsReplaced;
            super::visit (s);

            if (r != itemsReplaced)
                s.updateStructureMembers();

            return s;
        }

        AST::Expression& visit (AST::TypeCast& t) override
        {
            super::visit (t);

            if (requiresRemapping (t.targetType))
            {
                auto& remappedType = getRemappedType (t.context, t.targetType);

                if (auto args = cast<AST::CommaSeparatedList> (t.source))
                    return allocator.allocate<AST::CallOrCast> (remappedType, args, false);

                auto& args = allocator.allocate<AST::CommaSeparatedList> (t.context);
                args.items.push_back (t.source);
                args.items.push_back (allocator.allocate<AST::Constant> (t.context, soul::Value::createInt32 (0)));

                return allocator.allocate<AST::CallOrCast> (remappedType, args, false);
            }

            return t;
        }

        AST::Expression& visit (AST::Constant& c) override
        {
            super::visit (c);

            if (requiresRemapping (c.getResultType()))
            {
                auto& remappedType = getRemappedType (c.context, c.getResultType());
                auto& args = allocator.allocate<AST::CommaSeparatedList> (c.context);
                auto resultType = c.getResultType();

                if (c.getResultType().isComplex32())
                {
                    if (resultType.isVector())
                    {
                        ArrayWithPreallocation<Value, 8> realValues, imagValues;

                        for (size_t i = 0; i < resultType.getVectorSize(); i++)
                        {
                            auto v = c.value.getSlice (i, i + 1).getAsComplex32();

                            realValues.push_back (soul::Value (v.real()));
                            imagValues.push_back (soul::Value (v.imag()));
                        }

                        args.items.push_back (allocator.allocate<AST::Constant> (c.context, soul::Value::createArrayOrVector (soul::Type::createVector (PrimitiveType::float32, resultType.getVectorSize()), realValues)));
                        args.items.push_back (allocator.allocate<AST::Constant> (c.context, soul::Value::createArrayOrVector (soul::Type::createVector (PrimitiveType::float32, resultType.getVectorSize()), imagValues)));
                    }
                    else if (resultType.isArray())
                    {
                        for (size_t i = 0; i < resultType.getArraySize(); i++)
                        {
                            auto v = c.value.getSlice (i, i + 1).getAsComplex32();

                            auto& item = allocator.allocate<AST::CommaSeparatedList> (c.context);
                            item.items.push_back (allocator.allocate<AST::Constant> (c.context, soul::Value (v.real())));
                            item.items.push_back (allocator.allocate<AST::Constant> (c.context, soul::Value (v.imag())));

                            args.items.push_back (item);
                        }
                    }
                    else
                    {
                        auto v = c.value.getAsComplex32();
                        args.items.push_back (allocator.allocate<AST::Constant> (c.context, soul::Value (v.real())));
                        args.items.push_back (allocator.allocate<AST::Constant> (c.context, soul::Value (v.imag())));
                    }
                }
                else
                {
                    if (resultType.isVector())
                    {
                        ArrayWithPreallocation<Value, 8> realValues, imagValues;

                        for (size_t i = 0; i < resultType.getVectorSize(); i++)
                        {
                            auto v = c.value.getSlice (i, i + 1).getAsComplex64();

                            realValues.push_back (soul::Value (v.real()));
                            imagValues.push_back (soul::Value (v.imag()));
                        }

                        args.items.push_back (allocator.allocate<AST::Constant> (c.context, soul::Value::createArrayOrVector (soul::Type::createVector (PrimitiveType::float64, resultType.getVectorSize()), realValues)));
                        args.items.push_back (allocator.allocate<AST::Constant> (c.context, soul::Value::createArrayOrVector (soul::Type::createVector (PrimitiveType::float64, resultType.getVectorSize()), imagValues)));
                    }
                    else if (resultType.isArray())
                    {
                        for (size_t i = 0; i < resultType.getArraySize(); i++)
                        {
                            auto v = c.value.getSlice (i, i + 1).getAsComplex64();

                            auto& item = allocator.allocate<AST::CommaSeparatedList> (c.context);
                            item.items.push_back (allocator.allocate<AST::Constant> (c.context, soul::Value (v.real())));
                            item.items.push_back (allocator.allocate<AST::Constant> (c.context, soul::Value (v.imag())));

                            args.items.push_back (item);
                        }
                    }
                    else
                    {
                        auto v = c.value.getAsComplex64();
                        args.items.push_back (allocator.allocate<AST::Constant> (c.context, soul::Value (v.real())));
                        args.items.push_back (allocator.allocate<AST::Constant> (c.context, soul::Value (v.imag())));
                    }
                }

                return allocator.allocate<AST::CallOrCast> (remappedType, args, false);
            }

            return c;
        }

    private:
        std::unordered_map<std::string, Type> complexTypes;

        AST::Expression& getRemappedType (AST::Context& context, const soul::Type& type)
        {
            if (type.isPrimitive())
                return getRemappedType (context, type.isComplex32(), 1, 0, type.isReference(), type.isConst());

            if (type.isVector())
                return getRemappedType (context, type.isComplex32(), type.getVectorSize(), 0, type.isReference(), type.isConst());

            SOUL_ASSERT (type.isArray());
            return getRemappedType (context,
                                    type.getArrayElementType().isComplex32(),
                                    type.getArrayElementType().getVectorSize(),
                                    type.getArraySize(),
                                    type.isReference(),
                                    type.isConst());
        }

        AST::Expression& getRemappedType (AST::Context& context, bool is32Bit, size_t vectorSize, size_t arraySize, bool isReference, bool isConst)
        {
            auto complexType = getComplexType (context, is32Bit, vectorSize);

            if (arraySize != 0)
                complexType = complexType.createArray (arraySize);

            if (isReference)
                complexType = complexType.createReference();

            if (isConst)
                complexType = complexType.createConst();

            return allocator.allocate<AST::ConcreteType> (context, complexType);
        }

        pool_ptr<AST::ModuleBase> getModule (const soul::IdentifierPath& path) const
        {
            pool_ptr<AST::ModuleBase> result;

            AST::Scope::NameSearch search;

            search.partiallyQualifiedPath = path;
            search.findNamespaces = true;

            module.performFullNameSearch (search, nullptr);

            if (search.itemsFound.size() == 1)
            {
                auto item = search.itemsFound.front();

                if (auto e = cast<AST::ModuleBase> (item))
                    result = e;
            }

            return result;
        }

        Type getComplexType (AST::Context& context, bool is32Bit, size_t vectorSize)
        {
            int bits = is32Bit ? 32 : 64;
            std::string namespaceAlias = "c" + std::to_string (bits) + "_" + std::to_string (vectorSize);

            auto i = complexTypes.find (namespaceAlias);

            if (i != complexTypes.end())
                return i->second;

            // Create the namespace alias
            auto& specialisationArgs = allocator.allocate<AST::CommaSeparatedList> (context);
            specialisationArgs.items.push_back (allocator.allocate<AST::ConcreteType> (context, is32Bit ? PrimitiveType::float32 : PrimitiveType::float64));
            specialisationArgs.items.push_back (allocator.allocate<AST::Constant> (context, soul::Value (static_cast<int32_t> (vectorSize))));

            auto& n = allocator.allocate<AST::NamespaceAliasDeclaration> (context,
                                                                          allocator.get (namespaceAlias),
                                                                          allocator.allocate<AST::QualifiedIdentifier> (context, soul::IdentifierPath::fromString (allocator.identifiers, "soul::complex_lib::imp")),
                                                                          specialisationArgs);

            complexLib->namespaceAliases.push_back (n);
            complexLib->isFullyResolved = false;
            ResolutionPass::run (allocator, *complexLib, true);

            auto complexType = n.resolvedNamespace->structures.front()->resolveAsType();

            complexTypes[namespaceAlias] = complexType;

            return complexType;
        }
    };
};

} // namespace soul
