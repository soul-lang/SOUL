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

struct ASTUtilities
{
    static void mergeDuplicateNamespaces (AST::Namespace& ns)
    {
        while (mergeFirstPairOfDuplicateNamespaces (ns))
        {}
    }

    static void findAllMainProcessors (AST::ModuleBase& module, std::vector<pool_ref<AST::ProcessorBase>>& found)
    {
        for (auto& m : module.getSubModules())
        {
            if (auto pb = cast<AST::ProcessorBase> (m))
                if (auto main = pb->annotation.findProperty ("main"))
                    if (auto c = main->value->getAsConstant())
                        if (c->value.getAsBool())
                            found.push_back (*pb);

            findAllMainProcessors (m, found);
        }
    }

    static pool_ptr<AST::ProcessorBase> scanForProcessorToUseAsMain (AST::ModuleBase& module)
    {
        pool_ptr<AST::ProcessorBase> lastProcessor;

        for (auto& m : module.getSubModules())
        {
            auto p1 = cast<AST::ProcessorBase> (m);

            if (p1 != nullptr && ! p1->isSpecialisedInstance() && p1->annotation.findProperty ("main") == nullptr)
                lastProcessor = p1;
            else if (auto p2 = scanForProcessorToUseAsMain (m))
                lastProcessor = p2;
        }

        return lastProcessor;
    }

    static void findAllModulesToCompile (const AST::Namespace& parentNamespace,
                                         std::vector<pool_ref<AST::ModuleBase>>& modulesToCompile)
    {
        for (auto& m : parentNamespace.subModules)
        {
            SOUL_ASSERT (m->getSpecialisationParameters().empty());
            modulesToCompile.push_back (m);

            if (auto ns = cast<AST::Namespace> (m))
                findAllModulesToCompile (*ns, modulesToCompile);
        }
    }

    static void removeModulesWithSpecialisationParams (AST::Namespace& ns)
    {
        for (auto& m : ns.getSubModules())
            if (auto sub = cast<AST::Namespace> (m))
                removeModulesWithSpecialisationParams (*sub);

        removeIf (ns.subModules,
                  [] (pool_ref<AST::ModuleBase>& m) { return ! m->getSpecialisationParameters().empty(); });
    }

    static AST::WriteToEndpoint& getTopLevelWriteToEndpoint (AST::WriteToEndpoint& ws)
    {
        if (auto chainedWrite = cast<AST::WriteToEndpoint> (ws.target))
            return getTopLevelWriteToEndpoint (*chainedWrite);

        return ws;
    }

    static void resolveHoistedEndpoints (AST::Allocator& allocator, AST::ModuleBase& module)
    {
        for (auto& m : module.getSubModules())
            resolveHoistedEndpoints (allocator, m);

        if (auto graph = cast<AST::Graph> (module))
        {
            while (hoistFirstChildEndpoint (allocator, *graph))
            {}
        }
    }

    static const char* getConsoleEndpointInternalName()  { return "_console"; }

    static pool_ptr<AST::OutputEndpointRef> createConsoleEndpoint (AST::Allocator& allocator, AST::QualifiedIdentifier& name)
    {
        if (! (name.getPath().isUnqualifiedName ("console") || name.getPath().isUnqualifiedName ("consoul")))
            return {};

        SOUL_ASSERT (name.getParentScope() != nullptr);
        auto processor = name.getParentScope()->findProcessor();

        if (processor == nullptr)
            name.context.throwError (Errors::cannotFindOutput (name.getPath()));

        if (auto e = processor->findEndpoint (getConsoleEndpointInternalName(), false))
            return allocator.allocate<AST::OutputEndpointRef> (name.context, *e);

        auto& newDebugEndpoint = allocator.allocate<AST::EndpointDeclaration> (allocator, AST::Context(), false, EndpointType::event);
        newDebugEndpoint.name = allocator.get (getConsoleEndpointInternalName());
        newDebugEndpoint.needsToBeExposedInParent = true;
        newDebugEndpoint.isConsoleEndpoint = true;

        processor->endpoints.push_back (newDebugEndpoint);

        return allocator.allocate<AST::OutputEndpointRef> (name.context, newDebugEndpoint);
    }

    static bool isConsoleEndpoint (const AST::EndpointDeclaration& e)
    {
        return e.needsToBeExposedInParent && e.name == ASTUtilities::getConsoleEndpointInternalName();
    }

    static void ensureEventEndpointSupportsType (AST::Allocator& allocator, AST::EndpointDeclaration& endpoint, const Type& type)
    {
        if (type.isReference() || type.isConst())
            return ensureEventEndpointSupportsType (allocator, endpoint, type.withConstAndRefFlags (false, false));

        for (auto& t : endpoint.getDetails().getResolvedDataTypes())
            if (t.isEqual (type, Type::ComparisonFlags::ignoreConst | Type::ComparisonFlags::ignoreReferences))
                return;

        endpoint.getDetails().dataTypes.push_back (allocator.allocate<AST::ConcreteType> (AST::Context(), type));
    }

    static void connectAnyChildEndpointsNeedingToBeExposed (AST::Allocator& allocator, AST::ProcessorBase& processor)
    {
        if (auto g = cast<AST::Graph> (processor))
        {
            while (exposeChildEndpoints (allocator, *g))
            {}
        }
    }

    static AST::Expression& createEndpointRef (AST::Allocator& allocator, const AST::Context& c, AST::EndpointDeclaration& e)
    {
        if (e.isInput)
            return allocator.allocate<AST::InputEndpointRef> (c, e);

        return allocator.allocate<AST::OutputEndpointRef> (c, e);
    }

    static std::string getSignatureString (const void* o)   { return choc::text::createHexString (reinterpret_cast<std::uintptr_t> (o)); }

    static std::string getTypeArraySignature (const AST::TypeArray& types)
    {
        auto result = std::to_string (types.size());

        for (auto& t : types)
        {
            if (t.isStruct())
                result += "_" + getSignatureString (t.getStruct().get());
            else
                result += "_" + t.withConstAndRefFlags (false, false).getShortIdentifierDescription();
        }

        return result;
    }

    static std::string getFunctionSignature (const AST::Function& f)
    {
        AST::TypeArray types;

        for (auto& p : f.parameters)
            types.push_back (p->getType());

        return f.name.toString() + "_" + getTypeArraySignature (types);
    }

    template <typename ArgList, typename ParamList>
    static std::string getSpecialisationSignature (const ParamList& params, const ArgList& args)
    {
        std::stringstream key;

        for (size_t i = 0; i < params.size(); ++i)
        {
            auto& param = params[i];

            if (i > 0)
                key << ',';

            if (auto u = cast<AST::UsingDeclaration> (param))
            {
                auto targetType = (i < args.size()) ? args[i]->resolveAsType() : u->targetType->resolveAsType();

                if (targetType.isStruct())
                    key << getSignatureString (targetType.getStruct().get());
                else
                    key << targetType.getShortIdentifierDescription();

                continue;
            }

            if (auto v = cast<AST::VariableDeclaration> (param))
            {
                auto& value = (i < args.size()) ?  args[i]->getAsConstant()->value : v->initialValue->getAsConstant()->value;
                key << std::string (static_cast<const char*> (value.getPackedData()), value.getPackedDataSize());
                continue;
            }

            if (auto v = cast<AST::NamespaceAliasDeclaration> (param))
            {
                auto ns = (i < args.size()) ? cast<AST::NamespaceRef> (args[i])->ns : v->resolvedNamespace;
                key << getSignatureString (ns.get());
                continue;
            }

            if (auto v = cast<AST::ProcessorAliasDeclaration> (param))
            {
                key << getSignatureString (v->resolvedProcessor.get());
                continue;
            }

            SOUL_ASSERT_FALSE;
        }

        return key.str();
    }

    static AST::StaticAssertion& createStaticAssertion (const AST::Context& context, AST::Allocator& allocator,
                                                        ArrayView<pool_ref<AST::Expression>> args)
    {
        auto numArgs = args.size();

        if (numArgs != 1 && numArgs != 2)
            context.throwError (Errors::expected1or2Args());

        auto getError = [&] () -> std::string_view
        {
            if (numArgs == 2)
            {
                auto& e = args[1].get();

                if (AST::isResolvedAsConstant (e))
                    if (auto c = e.getAsConstant())
                        if (c->value.getType().isStringLiteral())
                            return allocator.stringDictionary.getStringForHandle (c->value.getStringLiteral());

                e.context.throwError (Errors::expectedStringLiteralAsArg2());
            }

            return {};
        };

        return allocator.allocate<AST::StaticAssertion> (context, args.front(), std::string (getError()));
    }

private:
    static void mergeNamespaces (AST::Namespace& target, AST::Namespace& source)
    {
        auto newParentScope = std::addressof (target);

        for (auto& f : source.functions)
        {
            target.functions.push_back (f);
            f->context.parentScope = newParentScope;
        }

        for (auto& s : source.structures)
        {
            target.structures.push_back (s);
            s->context.parentScope = newParentScope;
        }

        for (auto& u : source.usings)
        {
            target.usings.push_back (u);
            u->context.parentScope = newParentScope;
        }

        for (auto& m : source.subModules)
        {
            target.subModules.push_back (m);
            m->context.parentScope = newParentScope;
        }

        for (auto& c : source.constants)
        {
            target.constants.push_back (c);
            c->context.parentScope = newParentScope;
        }
    }

    static bool mergeFirstPairOfDuplicateNamespaces (AST::Namespace& ns)
    {
        bool anyDone = false;

        for (size_t i = 0; i < ns.subModules.size(); ++i)
        {
            if (auto ns1 = cast<AST::Namespace> (ns.subModules[i]))
            {
                anyDone = mergeFirstPairOfDuplicateNamespaces (*ns1) || anyDone;

                for (size_t j = i + 1; j < ns.subModules.size(); ++j)
                {
                    if (auto ns2 = cast<AST::Namespace> (ns.subModules[j]))
                    {
                        if (ns1->name == ns2->name
                             && ns1->getSpecialisationParameters().empty()
                             && ns2->getSpecialisationParameters().empty())
                        {
                            mergeNamespaces (*ns1, *ns2);
                            ns.subModules.erase (getIteratorForIndex (ns.subModules, j));
                            return true;
                        }
                    }
                }
            }
        }

        return anyDone;
    }

    static AST::EndpointDeclaration& findEndpoint (AST::ProcessorBase& processor, const AST::UnqualifiedName& name, bool isInput)
    {
        auto result = processor.findEndpoint (name.identifier, isInput);

        if (result == nullptr)
            name.context.throwError (isInput ? Errors::cannotFindInput (name.toString())
                                             : Errors::cannotFindOutput (name.toString()));

        return *result;
    }

    static std::string makeUniqueEndpointName (AST::ProcessorBase& parent,
                                               ArrayView<AST::ChildEndpointPath::PathSection> path)
    {
        std::string root = "expose";

        for (auto& p : path)
            root += "_" + p.name->toString();

        return addSuffixToMakeUnique (makeSafeIdentifierName (root),
                                      [&] (const std::string& nm) { return parent.findEndpoint (nm) != nullptr; });
    }

    //==============================================================================
    static AST::Connection::SharedEndpoint& createConnectionEndpoint (AST::Allocator& allocator, const AST::Context& c,
                                                                      pool_ptr<AST::ProcessorInstance> processor, const AST::EndpointDeclaration& endpoint)
    {
        pool_ptr<AST::ProcessorInstanceRef> processorRef;

        if (processor != nullptr)
            processorRef = allocator.allocate<AST::ProcessorInstanceRef> (c, *processor);

        auto& name = allocator.allocate<AST::UnqualifiedName> (c, endpoint.name);
        return allocator.allocate<AST::Connection::SharedEndpoint> (allocator.allocate<AST::ConnectionEndpointRef> (c, processorRef, name));
    }

    static void setupEndpointDetailsAndConnection (AST::Allocator& allocator,
                                                   AST::Graph& parentGraph,
                                                   AST::EndpointDeclaration& parentEndpoint,
                                                   AST::ProcessorInstance& childProcessorInstance,
                                                   AST::EndpointDeclaration& childEndpoint)
    {
        parentEndpoint.details = allocator.allocate<AST::EndpointDetails> (childEndpoint.getDetails());
        parentEndpoint.annotation.mergeProperties (childEndpoint.annotation);
        parentEndpoint.childPath.reset();

        auto& parent = createConnectionEndpoint (allocator, {}, {}, parentEndpoint);
        auto& child  = createConnectionEndpoint (allocator, {}, childProcessorInstance, childEndpoint);

        if (parentEndpoint.isInput)
            parentGraph.connections.push_back (allocator.allocate<AST::Connection> (AST::Context(), InterpolationType::none, parent, child, nullptr));
        else
            parentGraph.connections.push_back (allocator.allocate<AST::Connection> (AST::Context(), InterpolationType::none, child, parent, nullptr));
    }

    static void resolveEndpoint (AST::Allocator& allocator,
                                 AST::Graph& parentGraph,
                                 AST::EndpointDeclaration& hoistedEndpoint,
                                 ArrayView<AST::ChildEndpointPath::PathSection> path)
    {
        SOUL_ASSERT (path.size() > 1);

        auto childProcessorQualName = path.front().name;
        auto& nameContext = childProcessorQualName->context;
        auto childProcessorName = childProcessorQualName->identifier;
        auto childProcessorInstance = parentGraph.findChildProcessor (childProcessorName);

        if (childProcessorInstance == nullptr)
            nameContext.throwError (Errors::cannotFindProcessor (childProcessorName));

        if (childProcessorInstance->arraySize != nullptr)
            nameContext.throwError (Errors::notYetImplemented ("Exposing child endpoints involving processor arrays"));

        if (path.front().index != nullptr)
            nameContext.throwError (Errors::targetIsNotAnArray());

        auto& childProcessor = parentGraph.findSingleMatchingProcessor (*childProcessorInstance);
        auto childGraph = cast<AST::Graph> (childProcessor);

        if (path.size() == 2)
        {
            auto& childEndpoint = findEndpoint (childProcessor, *path.back().name, hoistedEndpoint.isInput);

            if (childEndpoint.isUnresolvedChildReference())
                resolveEndpoint (allocator, *childGraph, childEndpoint, childEndpoint.childPath->sections);

            if (path.back().index != nullptr)
                nameContext.throwError (Errors::targetIsNotAnArray());

            setupEndpointDetailsAndConnection (allocator, parentGraph, hoistedEndpoint, *childProcessorInstance, childEndpoint);
            return;
        }

        if (childGraph == nullptr)
            nameContext.throwError (Errors::cannotFindProcessor (childProcessorName));

        auto& newEndpointInChild = allocator.allocate<AST::EndpointDeclaration> (AST::Context(), hoistedEndpoint.isInput);
        newEndpointInChild.name = allocator.get (makeUniqueEndpointName (*childGraph, path));
        childGraph->endpoints.push_back (newEndpointInChild);

        resolveEndpoint (allocator, *childGraph, newEndpointInChild, path.tail());
        setupEndpointDetailsAndConnection (allocator, parentGraph, hoistedEndpoint, *childProcessorInstance, newEndpointInChild);
    }

    static bool hoistFirstChildEndpoint (AST::Allocator& allocator, AST::Graph& g)
    {
        for (size_t i = 0; i < g.endpoints.size(); ++i)
        {
            auto& e = g.endpoints[i];

            if (e->isUnresolvedChildReference())
            {
                resolveEndpoint (allocator, g, e, e->childPath->sections);
                return true;
            }
        }

        return false;
    }

    static bool exposeChildEndpoints (AST::Allocator& allocator, AST::Graph& graph)
    {
        bool anyChanges = false;

        for (auto& i : graph.processorInstances)
            if (auto childProcessor = i->targetProcessor->getAsProcessor())
                if (auto childGraph = cast<AST::Graph> (childProcessor))
                    if (exposeChildEndpoints (allocator, *childGraph))
                        anyChanges = true;

        for (auto& processorInstance : graph.processorInstances)
        {
            if (auto childProcessor = processorInstance->targetProcessor->getAsProcessor())
            {
                for (auto& childEndpoint : childProcessor->getEndpoints())
                {
                    if (childEndpoint->needsToBeExposedInParent)
                    {
                        auto parentEndpoint = graph.findEndpoint (childEndpoint->name, false);

                        if (parentEndpoint != nullptr)
                        {
                            for (auto& t : childEndpoint->getDetails().getResolvedDataTypes())
                                ensureEventEndpointSupportsType (allocator, *parentEndpoint, t);
                        }
                        else
                        {
                            parentEndpoint = allocator.allocate<AST::EndpointDeclaration> (AST::Context(), false);
                            parentEndpoint->name = allocator.get (childEndpoint->name);
                            parentEndpoint->details = allocator.allocate<AST::EndpointDetails> (childEndpoint->getDetails());
                            parentEndpoint->needsToBeExposedInParent = true;
                            graph.endpoints.push_back (*parentEndpoint);
                        }

                        auto& parent = allocator.allocate<AST::Connection::SharedEndpoint> (createEndpointRef (allocator, {}, *parentEndpoint));
                        auto& child = createConnectionEndpoint (allocator, {}, processorInstance, childEndpoint);

                        graph.connections.push_back (allocator.allocate<AST::Connection> (AST::Context(), InterpolationType::none,
                                                                                          child, parent, nullptr));
                        anyChanges = true;
                    }
                }
            }
        }

        if (anyChanges)
            for (auto& i : graph.processorInstances)
                if (auto childProcessor = i->targetProcessor->getAsProcessor())
                    for (auto& childEndpoint : childProcessor->getEndpoints())
                        childEndpoint->needsToBeExposedInParent = false;

        return anyChanges;
    }
};

}
