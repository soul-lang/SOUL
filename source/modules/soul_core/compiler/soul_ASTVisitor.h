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
/** Iterates over an AST object, invoking virtual methods for each type of sub-object
    that it encounters. If you need to replace the objects that are visited, use the
    RewritingASTVisitor instead.
*/
struct ASTVisitor
{
    virtual ~ASTVisitor() {}

    #define SOUL_INVOKE_FOR_SUBCLASS(ASTType) \
        case AST::ObjectType::ASTType:  visit (static_cast<AST::ASTType&> (t)); break;

    virtual void visitObject (AST::ModuleBase& t)
    {
        switch (t.objectType)
        {
            SOUL_AST_MODULES (SOUL_INVOKE_FOR_SUBCLASS)
            default: throwInternalCompilerError ("Unknown AST module"); break;
        }
    }

    virtual void visitObject (AST::Expression& t)
    {
        switch (t.objectType)
        {
            SOUL_AST_EXPRESSIONS (SOUL_INVOKE_FOR_SUBCLASS)
            default: throwInternalCompilerError ("Unknown AST expression"); break;
        }
    }

    virtual void visitObject (AST::Statement& t)
    {
        switch (t.objectType)
        {
            SOUL_AST_STATEMENTS (SOUL_INVOKE_FOR_SUBCLASS)
            default: throwInternalCompilerError ("Unknown AST statement"); break;
        }
    }

    virtual void visitObject (AST::ASTObject& t)
    {
        switch (t.objectType)
        {
            SOUL_AST_OBJECTS (SOUL_INVOKE_FOR_SUBCLASS)
            default: throwInternalCompilerError ("Unknown AST object"); break;
        }
    }

    template <typename ArrayType>
    void visitArray (ArrayType& array)
    {
        // use index iteration to survive cases where a visit causes the list to be mutated
        for (size_t i = 0; i < array.size(); ++i)
            visitObject (*array[i]);
    }

    #undef SOUL_INVOKE_FOR_SUBCLASS

    virtual void visit (AST::Processor& p)
    {
        visitArray (p.endpoints);
        visitArray (p.structures);
        visitArray (p.usings);
        visitArray (p.stateVariables);
        visitArray (p.functions);
    }

    virtual void visit (AST::Graph& g)
    {
        visitArray (g.endpoints);
        visitArray (g.processorInstances);
        visitArray (g.processorAliases);
        visitArray (g.connections);
        visitArray (g.constants);
    }

    virtual void visit (AST::Namespace& n)
    {
        visitArray (n.subModules);
        visitArray (n.structures);
        visitArray (n.usings);
        visitArray (n.constants);
        visitArray (n.functions);
    }

    virtual void visit (AST::Block& b)
    {
        for (auto& s : b.statements)
            visitObject (*s);
    }

    virtual void visit (AST::Constant&)
    {
    }

    virtual void visit (AST::Annotation& a)
    {
        for (auto& property : a.properties)
            visitObject (*property.value);
    }

    virtual void visit (AST::BinaryOperator& o)
    {
        visitObject (*o.lhs);
        visitObject (*o.rhs);
    }

    virtual void visit (AST::UnaryOperator& o)
    {
        visitObject (*o.source);
    }

    virtual void visit (AST::VariableDeclaration& v)
    {
        if (v.declaredType != nullptr)
            visitObject (*v.declaredType);

        if (v.initialValue != nullptr)
            visitObject (*v.initialValue);

        visit (v.annotation);
    }

    virtual void visit (AST::VariableRef&)
    {
    }

    virtual void visit (AST::Assignment& a)
    {
        visitObject (*a.target);
        visitObject (*a.newValue);
    }

    virtual void visit (AST::CallOrCast& c)
    {
        if (c.arguments != nullptr)
            visitObject (*c.arguments);

        visitObject (*c.nameOrType);
    }

    virtual void visit (AST::FunctionCall& c)
    {
        if (c.arguments != nullptr)
            visitObject (*c.arguments);
    }

    virtual void visit (AST::TypeCast& c)
    {
        if (c.source != nullptr)
            visitObject (*c.source);
    }

    virtual void visit (AST::ArrayElementRef& s)
    {
        visitObject (*s.object);
        visitObject (*s.startIndex);

        if (s.endIndex != nullptr)
            visitObject (*s.endIndex);
    }

    virtual void visit (AST::StructMemberRef& s)
    {
        visitObject (*s.object);
    }

    virtual void visit (AST::PreOrPostIncOrDec& p)
    {
        visitObject (*p.target);
    }

    virtual void visit (AST::ReturnStatement& r)
    {
        if (r.returnValue != nullptr)
            visitObject (*r.returnValue);
    }

    virtual void visit (AST::TernaryOp& o)
    {
        visitObject (*o.condition);
        visitObject (*o.trueBranch);
        visitObject (*o.falseBranch);
    }

    virtual void visit (AST::IfStatement& i)
    {
        visitObject (*i.condition);
        visitObject (*i.trueBranch);

        if (i.falseBranch != nullptr)
            visitObject (*i.falseBranch);
    }

    virtual void visit (AST::BreakStatement&)
    {
    }

    virtual void visit (AST::ContinueStatement&)
    {
    }

    virtual void visit (AST::LoopStatement& l)
    {
        if (l.iterator != nullptr)      visitObject (*l.iterator);
        if (l.body != nullptr)          visitObject (*l.body);
        if (l.condition != nullptr)     visitObject (*l.condition);
        if (l.numIterations != nullptr) visitObject (*l.numIterations);
    }

    virtual void visit (AST::NoopStatement&)
    {
    }

    virtual void visit (AST::WriteToEndpoint& w)
    {
        visitObject (*w.target);
        visitObject (*w.value);
    }

    virtual void visit (AST::ProcessorProperty&)
    {
    }

    virtual void visit (AST::AdvanceClock&)
    {
    }

    virtual void visit (AST::Function& f)
    {
        visitObject (*f.returnType);

        for (auto& v : f.parameters)
            visitObject (*v);

        if (f.block != nullptr)
            visitObject (*f.block);
    }

    virtual void visit (AST::ConcreteType&)
    {
    }

    virtual void visit (AST::StructDeclaration& s)
    {
        for (auto& m : s.getMembers())
            visitObject (*m.type);
    }

    virtual void visit (AST::UsingDeclaration& u)
    {
        if (u.targetType != nullptr)
            visitObject (*u.targetType);
    }

    virtual void visit (AST::SubscriptWithBrackets& s)
    {
        visitObject (*s.lhs);

        if (s.rhs != nullptr)
            visitObject (*s.rhs);
    }

    virtual void visit (AST::SubscriptWithChevrons& s)
    {
        visitObject (*s.lhs);
        visitObject (*s.rhs);
    }

    virtual void visit (AST::TypeMetaFunction& c)
    {
        visitObject (*c.source);
    }

    virtual void visit (AST::ProcessorAliasDeclaration&)
    {
    }

    virtual void visit (AST::EndpointDeclaration& e)
    {
        if (e.details != nullptr)
        {
            for (auto& type : e.details->sampleTypes)
                visitObject (*type);

            if (e.details->arraySize != nullptr)
                visitObject (*e.details->arraySize);
        }

        if (e.childPath != nullptr)
            for (auto& p : e.childPath->sections)
                if (p.index != nullptr)
                    visitObject (*p.index);

        visit (e.annotation);
    }

    virtual void visit (AST::InputEndpointRef&)
    {
    }

    virtual void visit (AST::OutputEndpointRef&)
    {
    }

    virtual void visit (AST::Connection& c)
    {
        if (c.delayLength != nullptr)
            visitObject (*c.delayLength);
    }

    virtual void visit (AST::ProcessorInstance& i)
    {
        visitObject (*i.targetProcessor);

        for (auto& a : i.specialisationArgs)
            visitObject (*a);

        if (i.clockMultiplierRatio != nullptr)
            visitObject (*i.clockMultiplierRatio);

        if (i.clockDividerRatio != nullptr)
            visitObject (*i.clockDividerRatio);
    }

    virtual void visit (AST::ProcessorRef&)
    {
    }

    virtual void visit (AST::CommaSeparatedList& l)
    {
        for (auto& i : l.items)
            visitObject (*i);
    }

    virtual void visit (AST::QualifiedIdentifier&)
    {
    }

    virtual void visit (AST::DotOperator& o)
    {
        visitObject (*o.lhs);
    }

    virtual void visit (AST::StaticAssertion& a)
    {
        visitObject (*a.condition);
    }
};

//==============================================================================
/** Iterates over an AST object, invoking virtual methods for each type of sub-object
    that it encounters, and these methods can return a new replacement object in order
    to modify the tree.
*/
struct RewritingASTVisitor
{
    virtual ~RewritingASTVisitor() {}

    #define SOUL_INVOKE_FOR_SUBCLASS(ASTType) \
        case AST::ObjectType::ASTType:  return visit (static_cast<AST::ASTType&> (t));

    pool_ptr<AST::ModuleBase> visitObject (AST::ModuleBase& t)
    {
        switch (t.objectType)
        {
            SOUL_AST_MODULES (SOUL_INVOKE_FOR_SUBCLASS)
            default: throwInternalCompilerError ("Unknown AST module"); return {};
        }
    }

    AST::ExpPtr visitObject (AST::Expression& t)
    {
        switch (t.objectType)
        {
            SOUL_AST_EXPRESSIONS (SOUL_INVOKE_FOR_SUBCLASS)
            default: throwInternalCompilerError ("Unknown AST expression"); return {};
        }
    }

    AST::StatementPtr visitObject (AST::Statement& t)
    {
        switch (t.objectType)
        {
            SOUL_AST_STATEMENTS (SOUL_INVOKE_FOR_SUBCLASS)
            default: throwInternalCompilerError ("Unknown AST statement"); return {};
        }
    }

    pool_ptr<AST::ASTObject> visitObject (AST::ASTObject& t)
    {
        switch (t.objectType)
        {
            SOUL_AST_OBJECTS (SOUL_INVOKE_FOR_SUBCLASS)
            default: throwInternalCompilerError ("Unknown AST object"); return {};
        }
    }

    #undef SOUL_INVOKE_FOR_SUBCLASS

    template <typename Type>
    pool_ptr<Type> visitAs (pool_ptr<Type> o)
    {
        if (o == nullptr)
            return {};

        return static_pool_cast<Type> (visitObject (*o));
    }

    size_t itemsReplaced = 0;

    template <typename Type>
    void replace (pool_ptr<Type>& dest, pool_ptr<Type> newValue) noexcept
    {
        if (dest != newValue)
        {
            ++itemsReplaced;
            dest = newValue;
        }
    }

    template <typename Type>
    void replaceAs (pool_ptr<Type>& o)               { replace (o, visitAs<Type> (o)); }
    void replaceExpression (AST::ExpPtr& e)          { replace (e, visitExpression (e)); }
    void replaceStatement (AST::StatementPtr& s)     { replace (s, visitStatement (s)); }

    AST::StatementPtr visitStatement (AST::StatementPtr s)
    {
        return visitAs<AST::Statement> (s);
    }

    virtual AST::ExpPtr visitExpression (AST::ExpPtr e)
    {
        return visitAs<AST::Expression> (e);
    }

    template <typename ArrayType>
    void visitArray (ArrayType& array)
    {
        // use index iteration to survive cases where a visit causes the list to be mutated
        for (size_t i = 0; i < array.size(); ++i)
            visitObject (*array[i]);
    }

    template <typename ArrayType>
    void replaceArray (ArrayType& array)
    {
        // this gubbins is all needed to survive cases where a visit causes the list to be mutated
        for (size_t i = 0; i < array.size(); ++i)
        {
            auto oldObject = array[i];
            auto newObject = visitAs<typename ArrayType::value_type::ObjectType> (*oldObject);

            if (oldObject != newObject)
            {
                array[i] = newObject;
                ++itemsReplaced;
            }
        }
    }

    //==============================================================================
    virtual AST::ProcessorPtr visit (AST::Processor& p)
    {
        visitArray (p.endpoints);
        visitArray (p.structures);
        visitArray (p.stateVariables);
        replaceArray (p.functions);

        return p;
    }

    virtual AST::GraphPtr visit (AST::Graph& g)
    {
        visitArray (g.endpoints);
        visitArray (g.processorInstances);
        visitArray (g.processorAliases);
        visitArray (g.connections);
        visitArray (g.constants);

        return g;
    }

    virtual AST::NamespacePtr visit (AST::Namespace& n)
    {
        visitArray   (n.subModules);
        visitArray   (n.structures);
        replaceArray (n.usings);
        visitArray   (n.constants);
        replaceArray (n.functions);

        return n;
    }

    virtual AST::BlockPtr visit (AST::Block& b)
    {
        for (auto& s : b.statements)
            replaceStatement (s);

        return b;
    }

    virtual AST::ExpPtr visit (AST::Constant& o)
    {
        return o;
    }

    virtual void visit (AST::Annotation& a)
    {
        for (auto& property : a.properties)
            replaceExpression (property.value);
    }

    virtual AST::ExpPtr visit (AST::BinaryOperator& o)
    {
        replaceExpression (o.lhs);
        replaceExpression (o.rhs);
        return o;
    }

    virtual AST::ExpPtr visit (AST::UnaryOperator& o)
    {
        replaceExpression (o.source);
        return o;
    }

    virtual AST::StatementPtr visit (AST::VariableDeclaration& v)
    {
        if (v.declaredType != nullptr)  replaceExpression (v.declaredType);
        if (v.initialValue != nullptr)  replaceExpression (v.initialValue);

        visit (v.annotation);
        return v;
    }

    virtual AST::ExpPtr visit (AST::VariableRef& o)
    {
        return o;
    }

    virtual AST::ExpPtr visit (AST::Assignment& a)
    {
        replaceExpression (a.target);
        replaceExpression (a.newValue);
        return a;
    }

    virtual AST::ExpPtr visit (AST::CallOrCast& c)
    {
        if (c.arguments != nullptr)
            visitObject (*c.arguments);

        replaceExpression (c.nameOrType);
        return c;
    }

    virtual AST::ExpPtr visit (AST::FunctionCall& c)
    {
        if (c.arguments != nullptr)
            visitObject (*c.arguments);

        return c;
    }

    virtual AST::ExpPtr visit (AST::TypeCast& c)
    {
        if (c.source != nullptr)
            replaceExpression (c.source);

        return c;
    }

    virtual AST::ExpPtr visit (AST::ArrayElementRef& s)
    {
        replaceExpression (s.object);
        if (s.startIndex != nullptr)  replaceExpression (s.startIndex);
        if (s.endIndex != nullptr)    replaceExpression (s.endIndex);
        return s;
    }

    virtual AST::ExpPtr visit (AST::StructMemberRef& s)
    {
        replaceExpression (s.object);
        return s;
    }

    virtual AST::ExpPtr visit (AST::PreOrPostIncOrDec& p)
    {
        replaceExpression (p.target);
        return p;
    }

    virtual AST::StatementPtr visit (AST::ReturnStatement& r)
    {
        replaceExpression (r.returnValue);
        return r;
    }

    virtual AST::ExpPtr visit (AST::TernaryOp& o)
    {
        replaceExpression (o.condition);
        replaceExpression (o.trueBranch);
        replaceExpression (o.falseBranch);
        return o;
    }

    virtual AST::StatementPtr visit (AST::IfStatement& i)
    {
        replaceExpression (i.condition);
        replaceStatement (i.trueBranch);
        replaceStatement (i.falseBranch);
        return i;
    }

    virtual AST::StatementPtr visit (AST::BreakStatement& o)
    {
        return o;
    }

    virtual AST::StatementPtr visit (AST::ContinueStatement& o)
    {
        return o;
    }

    virtual AST::StatementPtr visit (AST::LoopStatement& l)
    {
        replaceExpression (l.condition);
        replaceExpression (l.numIterations);
        replaceStatement (l.iterator);
        replaceStatement (l.body);
        return l;
    }

    virtual AST::StatementPtr visit (AST::NoopStatement& o)
    {
        return o;
    }

    virtual AST::ExpPtr visit (AST::WriteToEndpoint& w)
    {
        replaceExpression (w.target);
        replaceExpression (w.value);
        return w;
    }

    virtual AST::ExpPtr visit (AST::ProcessorProperty& p)
    {
        return p;
    }

    virtual AST::ExpPtr visit (AST::AdvanceClock& o)
    {
        return o;
    }

    virtual AST::FunctionPtr visit (AST::Function& f)
    {
        replaceExpression (f.returnType);

        for (auto& v : f.parameters)
            replaceAs<AST::VariableDeclaration> (v);

        replaceAs<AST::Block> (f.block);
        return f;
    }

    virtual AST::ExpPtr visit (AST::ConcreteType& t)
    {
        return t;
    }

    virtual AST::StructDeclarationPtr visit (AST::StructDeclaration& s)
    {
        for (auto& m : s.getMembers())
            replaceExpression (m.type);

        return s;
    }

    virtual AST::UsingDeclarationPtr visit (AST::UsingDeclaration& u)
    {
        replaceExpression (u.targetType);
        return u;
    }

    virtual AST::ExpPtr visit (AST::SubscriptWithBrackets& s)
    {
        replaceExpression (s.lhs);
        replaceExpression (s.rhs);
        return s;
    }

    virtual AST::ExpPtr visit (AST::SubscriptWithChevrons& s)
    {
        replaceExpression (s.lhs);
        replaceExpression (s.rhs);
        return s;
    }

    virtual AST::ExpPtr visit (AST::TypeMetaFunction& c)
    {
        replaceExpression (c.source);
        return c;
    }

    virtual AST::ProcessorAliasDeclarationPtr visit (AST::ProcessorAliasDeclaration& a)
    {
        return a;
    }

    virtual AST::ASTObjectPtr visit (AST::EndpointDeclaration& e)
    {
        if (e.details != nullptr)
        {
            for (auto& type : e.details->sampleTypes)
                replaceExpression (type);

            replaceExpression (e.details->arraySize);
        }

        if (e.childPath != nullptr)
            for (auto& p : e.childPath->sections)
                replaceExpression (p.index);

        visit (e.annotation);
        return e;
    }

    virtual AST::ExpPtr visit (AST::InputEndpointRef& e)
    {
        return e;
    }

    virtual AST::ExpPtr visit (AST::OutputEndpointRef& e)
    {
        return e;
    }

    virtual AST::ConnectionPtr visit (AST::Connection& c)
    {
        replaceExpression (c.delayLength);
        return c;
    }

    virtual AST::ProcessorInstancePtr visit (AST::ProcessorInstance& i)
    {
        replaceExpression (i.targetProcessor);

        for (auto& a : i.specialisationArgs)
            replaceExpression (a);

        if (i.clockMultiplierRatio != nullptr)
            replaceExpression (i.clockMultiplierRatio);

        if (i.clockDividerRatio != nullptr)
            replaceExpression (i.clockDividerRatio);

        return i;
    }

    virtual AST::ProcessorRefPtr visit (AST::ProcessorRef& pr)
    {
        return pr;
    }

    virtual AST::ExpPtr visit (AST::CommaSeparatedList& l)
    {
        for (auto& i : l.items)
            replaceExpression (i);

        return l;
    }

    virtual AST::ExpPtr visit (AST::QualifiedIdentifier& o)
    {
        return o;
    }

    virtual AST::ExpPtr visit (AST::DotOperator& o)
    {
        replaceExpression (o.lhs);
        return o;
    }

    virtual AST::StaticAssertionPtr visit (AST::StaticAssertion& a)
    {
        replaceExpression (a.condition);
        return a;
    }
};

} // namespace soul
