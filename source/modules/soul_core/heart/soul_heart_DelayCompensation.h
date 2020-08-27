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
    Inserts delays into connections in a graph in order to correct for any
    internal delays on its child processors.
*/
struct DelayCompensation
{
    // Adjusts delays on the connections in the module, and returns the final overall latency.
    static uint32_t apply (Module& module)
    {
        if (module.isGraph())
        {
            DelayCompensation dc (module);

            if (! dc.buildNodes())
                return 0;

            dc.calculateInputLatenciesForAllNodes();
            dc.addCompensatoryDelaysOnConnections();
        }
        else
        {
            SOUL_ASSERT (module.isProcessor());
        }

        return module.latency;
    }

private:
    DelayCompensation (Module& g) : graph (g) {}

    struct Node
    {
        struct Source
        {
            Node& node;
            const heart::Connection& connection;
        };

        const heart::ProcessorInstance& processorInstance;
        uint32_t internalLatency = 0, absoluteLatencyAtInput = 0;
        ArrayWithPreallocation<Source, 4> sources;
    };

    Module& graph;
    std::vector<Node> nodes;
    std::vector<const Node*> visitedStack;

    Node* findNode (pool_ptr<heart::ProcessorInstance> p)
    {
        if (p != nullptr)
            for (auto& n : nodes)
                if (std::addressof (n.processorInstance) == std::addressof (*p))
                    return std::addressof (n);

        return {};
    }

    bool buildNodes()
    {
        if (graph.connections.empty())
            return false;

        nodes.reserve (graph.processorInstances.size());
        bool anyNodesWithInternalLatency = false;

        for (auto& instance : graph.processorInstances)
        {
            auto latency = apply (graph.program.getModuleWithName (instance->sourceName));
            nodes.push_back ({ instance, latency });

            if (latency != 0)
                anyNodesWithInternalLatency = true;
        }

        if (! anyNodesWithInternalLatency)
            return false; // skip building the connections if there's nothing to do..

        for (auto& c : graph.connections)
            if (auto src = findNode (c->source.processor))
                if (auto dst = findNode (c->dest.processor))
                    dst->sources.push_back ({ *src, c });

        return true;
    }

    void calculateInputLatenciesForAllNodes()
    {
        graph.latency = 0;
        visitedStack.reserve (256);

        for (auto& c : graph.connections)
        {
            if (c->dest.processor == nullptr)
            {
                if (auto src = findNode (c->source.processor))
                {
                    visitedStack.clear();
                    graph.latency = std::max (graph.latency, calculateAbsoluteLatencyOutOfNode (*src));
                }
            }
        }
    }

    void addCompensatoryDelaysOnConnections()
    {
        for (auto& c : graph.connections)
        {
            uint32_t latencyAtStartOfConnection = 0;
            uint32_t latencyAtEndOfConnection = graph.latency;

            if (auto src = findNode (c->source.processor))
                latencyAtStartOfConnection = src->absoluteLatencyAtInput + src->internalLatency;

            if (auto dst = findNode (c->dest.processor))
                latencyAtEndOfConnection = dst->absoluteLatencyAtInput;

            if (latencyAtEndOfConnection > latencyAtStartOfConnection)
                c->delayLength = c->delayLength.value_or (0) + (latencyAtEndOfConnection - latencyAtStartOfConnection);
        }
    }

    uint32_t calculateAbsoluteLatencyOutOfNode (Node& node)
    {
        if (std::find (visitedStack.begin(), visitedStack.end(), std::addressof (node)) != visitedStack.end())
            return node.internalLatency;

        visitedStack.push_back (std::addressof (node));
        auto maxSourceLatency = node.absoluteLatencyAtInput;

        for (auto& source : node.sources)
            maxSourceLatency = std::max (maxSourceLatency, calculateAbsoluteLatencyOutOfNode (source.node));

        visitedStack.pop_back();
        node.absoluteLatencyAtInput = maxSourceLatency;
        return maxSourceLatency + node.internalLatency;
    }
};

}
