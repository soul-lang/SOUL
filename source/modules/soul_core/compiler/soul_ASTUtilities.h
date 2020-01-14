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

    static void findAllMainProcessors (AST::ModuleBase& module, std::vector<pool_ptr<AST::ProcessorBase>>& found)
    {
        for (auto& m : module.getSubModules())
        {
            if (auto pb = cast<AST::ProcessorBase> (m))
                if (auto main = pb->annotation.findProperty ("main"))
                    if (auto c = main->value->getAsConstant())
                        if (c->value.getAsBool())
                            found.push_back (pb);

            findAllMainProcessors (*m, found);
        }
    }

    static pool_ptr<AST::ProcessorBase> scanForProcessorToUseAsMain (AST::ModuleBase& module)
    {
        pool_ptr<AST::ProcessorBase> lastProcessor;

        for (auto& m : module.getSubModules())
        {
            auto p1 = cast<AST::ProcessorBase> (m);

            if (p1 != nullptr && ! p1->annotation.findProperty ("main"))
                lastProcessor = p1;
            else if (auto p2 = scanForProcessorToUseAsMain (*m))
                lastProcessor = p2;
        }

        return lastProcessor;
    }

    static void findAllModulesToCompile (const AST::Namespace& parentNamespace, std::vector<pool_ptr<AST::ModuleBase>>& modulesToCompile)
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
                  [] (AST::ModuleBasePtr& m) { return ! m->getSpecialisationParameters().empty(); });
    }

    static void removeUnusedGraphs (AST::Namespace& ns, ArrayView<pool_ptr<AST::ProcessorBase>> graphsToKeep)
    {
        for (auto& m : ns.subModules)
            if (auto subNamespace = cast<AST::Namespace> (m))
                removeUnusedGraphs (*subNamespace, graphsToKeep);

        removeIf (ns.subModules, [=] (AST::ModuleBasePtr m)
        {
            if (auto graph = cast<AST::Graph> (m))
                return ! contains (graphsToKeep, graph);

            return false;
        });
    }

    static void resolveHoistedEndpoints (AST::Allocator& allocator, AST::ModuleBase& module)
    {
        for (auto& m : module.getSubModules())
            resolveHoistedEndpoints (allocator, *m);

        if (auto graph = cast<AST::Graph> (module))
        {
            while (hoistFirstChildEndpoint (allocator, *graph))
            {}
        }
    }

    static const char* getConsoleEndpointInternalName()  { return "_console"; }

    static pool_ptr<AST::OutputEndpointRef> createConsoleEndpoint (AST::Allocator& allocator, AST::QualifiedIdentifier& name)
    {
        if (! (name.path.isUnqualifiedName ("console") || name.path.isUnqualifiedName ("consoul")))
            return {};

        SOUL_ASSERT (name.getParentScope() != nullptr);
        auto processor = name.getParentScope()->findProcessor();

        if (processor == nullptr)
            name.context.throwError (Errors::cannotFindOutput (name.path));

        if (auto e = processor->findEndpoint (getConsoleEndpointInternalName(), false))
            return allocator.allocate<AST::OutputEndpointRef> (name.context, *e);

        auto& newDebugEndpoint = allocator.allocate<AST::EndpointDeclaration> (AST::Context(), false);
        newDebugEndpoint.name = allocator.get (getConsoleEndpointInternalName());
        newDebugEndpoint.details = std::make_unique<AST::EndpointDetails> (EndpointKind::event);
        newDebugEndpoint.needsToBeExposedInParent = true;
        processor->endpoints.push_back (newDebugEndpoint);

        return allocator.allocate<AST::OutputEndpointRef> (name.context, newDebugEndpoint);
    }

    static bool isConsoleEndpoint (const AST::EndpointDeclaration& e)
    {
        return e.needsToBeExposedInParent && e.name == ASTUtilities::getConsoleEndpointInternalName();
    }

    static void ensureEventEndpointHasSampleType (AST::Allocator& allocator, AST::EndpointDeclaration& endpoint, const Type& type)
    {
        if (type.isReference() || type.isConst())
            return ensureEventEndpointHasSampleType (allocator, endpoint, type.withConstAndRefFlags (false, false));

        for (auto& t : endpoint.details->getResolvedSampleTypes())
            if (t.isEqual (type, Type::ComparisonFlags::ignoreConst | Type::ComparisonFlags::ignoreReferences))
                return;

        endpoint.details->sampleTypes.push_back (allocator.allocate<AST::ConcreteType> (AST::Context(), type));
    }

    static void connectAnyChildEndpointsNeedingToBeExposed (AST::Allocator& allocator, AST::ProcessorBase& processor)
    {
        if (auto g = cast<AST::Graph> (processor))
        {
            while (exposeChildEndpoints (allocator, *g))
            {}
        }
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
                            ns.subModules.erase (ns.subModules.begin() + (ssize_t) j);
                            return true;
                        }
                    }
                }
            }
        }

        return anyDone;
    }

    static AST::EndpointDeclaration& findEndpoint (AST::ProcessorBase& processor, AST::QualifiedIdentifier& name, bool isInput)
    {
        auto result = processor.findEndpoint (name.path.getFirstPart(), isInput);

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
            root += "_" + p.name->path.toString();

        return addSuffixToMakeUnique (makeSafeIdentifierName (root),
                                      [&] (const std::string& nm) { return parent.findEndpoint (nm) != nullptr; });
    }

    //==============================================================================
    static void setupEndpointDetailsAndConnection (AST::Allocator& allocator,
                                                   AST::Graph& parentGraph,
                                                   AST::EndpointDeclaration& parentEndpoint,
                                                   AST::ProcessorInstance& childProcessor,
                                                   AST::EndpointDeclaration& childEndpoint)
    {
        parentEndpoint.details = std::make_unique<AST::EndpointDetails> (*childEndpoint.details);
        parentEndpoint.annotation.setProperties (childEndpoint.annotation);
        parentEndpoint.childPath.reset();

        AST::Connection::NameAndEndpoint parent, child;

        parent.processorName = allocator.allocate<AST::QualifiedIdentifier> (AST::Context(), IdentifierPath());
        parent.processorIndex = {};
        parent.endpoint = parentEndpoint.name;
        parent.endpointIndex = {};

        child.processorName = childProcessor.instanceName;
        child.processorIndex = {};
        child.endpoint = childEndpoint.name;
        child.endpointIndex = {};

        if (parentEndpoint.isInput)
            parentGraph.connections.push_back (allocator.allocate<AST::Connection> (AST::Context(), InterpolationType::none, parent, child, nullptr));
        else
            parentGraph.connections.push_back (allocator.allocate<AST::Connection> (AST::Context(), InterpolationType::none, child, parent, nullptr));
    }

    static  void resolveEndpoint (AST::Allocator& allocator,
                                  AST::Graph& parentGraph,
                                  AST::EndpointDeclaration& hoistedEndpoint,
                                  ArrayView<AST::ChildEndpointPath::PathSection> path)
    {
        SOUL_ASSERT (path.size() > 1);

        auto childProcessorQualName = path.front().name;
        auto& nameContext = childProcessorQualName->context;
        auto childProcessorName = childProcessorQualName->path.getFirstPart();
        auto childProcessorInstance = parentGraph.findChildProcessor (childProcessorName);

        if (childProcessorInstance == nullptr)
            nameContext.throwError (Errors::cannotFindProcessor (childProcessorName.toString()));

        if (childProcessorInstance->arraySize != nullptr)
            nameContext.throwError (Errors::notYetImplemented ("Exposing child endpoints involving processor arrays"));

        if (path.front().index != nullptr)
            nameContext.throwError (Errors::targetIsNotAnArray());

        auto& childProcessor = parentGraph.findSingleMatchingProcessor (*childProcessorInstance);
        auto childGraph = cast<AST::Graph> (childProcessor);

        if (path.size() == 2)
        {
            auto& childEndpoint = findEndpoint (childProcessor, *path.back().name, hoistedEndpoint.isInput);

            if (childEndpoint.details == nullptr)
                resolveEndpoint (allocator, *childGraph, childEndpoint, childEndpoint.childPath->sections);

            if (childEndpoint.details->arraySize != nullptr)
                nameContext.throwError (Errors::notYetImplemented ("Exposing child endpoint arrays"));

            if (path.back().index != nullptr)
                nameContext.throwError (Errors::targetIsNotAnArray());

            setupEndpointDetailsAndConnection (allocator, parentGraph, hoistedEndpoint, *childProcessorInstance, childEndpoint);
            return;
        }

        if (childGraph == nullptr)
            nameContext.throwError (Errors::cannotFindProcessor (childProcessorName.toString()));

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
            auto& e = *g.endpoints[i];

            if (e.details == nullptr)
            {
                resolveEndpoint (allocator, g, e, e.childPath->sections);
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

        for (auto& i : graph.processorInstances)
        {
            if (auto childProcessor = i->targetProcessor->getAsProcessor())
            {
                for (auto& childEndpoint : childProcessor->getEndpoints())
                {
                    if (childEndpoint->needsToBeExposedInParent)
                    {
                        auto parentEndpoint = graph.findEndpoint (childEndpoint->name, false);

                        if (parentEndpoint != nullptr)
                        {
                            for (auto& t : childEndpoint->details->getResolvedSampleTypes())
                                ensureEventEndpointHasSampleType (allocator, *parentEndpoint, t);
                        }
                        else
                        {
                            parentEndpoint = allocator.allocate<AST::EndpointDeclaration> (AST::Context(), false);
                            parentEndpoint->name = allocator.get (childEndpoint->name);
                            parentEndpoint->details = std::make_unique<AST::EndpointDetails> (*childEndpoint->details);
                            parentEndpoint->needsToBeExposedInParent = true;
                            graph.endpoints.push_back (parentEndpoint);
                        }

                        AST::Connection::NameAndEndpoint parent, child;

                        parent.processorName = allocator.allocate<AST::QualifiedIdentifier> (AST::Context(), IdentifierPath());
                        parent.processorIndex = {};
                        parent.endpoint = parentEndpoint->name;
                        parent.endpointIndex = {};

                        child.processorName = i->instanceName;
                        child.processorIndex = {};
                        child.endpoint = allocator.get (childEndpoint->name);
                        child.endpointIndex = {};

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
