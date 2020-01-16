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
    A Module represents a compiled version of a processor, graph, or namespace.
    Every Module object is created by and owned by a Program.
*/
class Module  final
{
public:
    bool isProcessor() const noexcept;
    bool isGraph() const noexcept;
    bool isNamespace() const noexcept;

    std::string moduleName;
    std::string getNameWithoutRootNamespace() const;
    std::string getNameWithoutRootNamespaceOrSpecialisations() const;

    std::vector<pool_ref<heart::InputDeclaration>> inputs;
    std::vector<pool_ref<heart::OutputDeclaration>> outputs;

    // Properties if it's a connection graph:
    std::vector<pool_ref<heart::Connection>> connections;
    std::vector<pool_ref<heart::ProcessorInstance>> processorInstances;

    // Properties if it's a processor
    std::vector<pool_ref<heart::Variable>> stateVariables;
    std::vector<pool_ref<heart::Function>> functions;
    std::vector<StructurePtr> structs;

    Annotation annotation;
    double sampleRate = 0;

    //==============================================================================
    heart::Allocator& allocator;

    template <typename Type, typename... Args>
    Type& allocate (Args&&... args)         { return allocator.allocate<Type> (std::forward<Args> (args)...); }

    //==============================================================================
    std::vector<pool_ptr<heart::Function>> getExportedFunctions() const;
    pool_ptr<heart::Function> findRunFunction() const;
    heart::Function& getRunFunction() const;
    heart::Function& getFunction (const std::string& name) const;
    pool_ptr<heart::Function> findFunction (const std::string& name) const;
    pool_ptr<heart::Variable> findStateVariable (const std::string& name) const;

    //==============================================================================
    pool_ptr<heart::InputDeclaration>  findInput  (const std::string& name) const;
    pool_ptr<heart::OutputDeclaration> findOutput (const std::string& name) const;

    //==============================================================================
    void addStruct (StructurePtr newStruct);
    Structure& addStruct (std::string name);
    Structure& findOrAddStruct (Identifier name);

    template <typename StringOrIdentifier>
    StructurePtr findStruct (const StringOrIdentifier& name) const noexcept
    {
        for (auto& s : structs)
            if (name == s->name)
                return s;

        return {};
    }

    //==============================================================================
    void rebuildBlockPredecessors();
    void rebuildVariableUseCounts();

private:
    friend class Program;

    int moduleId = 0;

    enum class ModuleType
    {
        processorModule,
        graphModule,
        namespaceModule
    };

    const ModuleType moduleType;

    Module() = delete;
    Module (Module&&) = delete;
    Module& operator= (Module&&) = delete;
    Module& operator= (const Module&) = delete;

    Module (heart::Allocator&, ModuleType);
    Module (heart::Allocator&, const Module& toClone);

    friend class PoolAllocator;

    static pool_ptr<Module> createProcessor (heart::Allocator&);
    static pool_ptr<Module> createGraph     (heart::Allocator&);
    static pool_ptr<Module> createNamespace (heart::Allocator&);
};


} // namespace soul
