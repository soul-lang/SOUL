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
    //==============================================================================
    class Functions
    {
    public:
        Functions (Module& m) : module (m) {}

        size_t size() const;
        ArrayView<pool_ref<heart::Function>> get() const;
        pool_ptr<heart::Function> findRunFunction() const;
        heart::Function& getRunFunction() const;
        heart::Function& get (std::string_view name) const;
        heart::Function& at (size_t index) const;
        pool_ptr<heart::Function> find (std::string_view name) const;
        heart::Function& add (std::string name, bool isEventFunction);
        heart::Function& add (const heart::InputDeclaration&, const Type&);
        bool remove (heart::Function&);
        bool contains (const heart::Function&) const;

        template <typename Predicate>
        bool removeIf (Predicate&& pred)
        {
            return soul::removeIf (functions, std::move (pred));
        }

    private:
        ArrayWithPreallocation<pool_ref<heart::Function>, 32> functions;
        Module& module;
    };

    //==============================================================================
    class StateVariables
    {
    public:
        size_t size() const;
        ArrayView<pool_ref<heart::Variable>> get() const;
        pool_ptr<heart::Variable> find (std::string_view name) const;
        void add (heart::Variable&);
        void clear();

    private:
        ArrayWithPreallocation<pool_ref<heart::Variable>, 32> stateVariables;
    };

    //==============================================================================
    class Structs
    {
    public:
        size_t size() const;
        ArrayView<StructurePtr> get() const;
        Structure& add (std::string name);
        Structure& add (Structure&);
        Structure& addCopy (Structure&);
        Structure& findOrAdd (std::string name);
        StructurePtr find (std::string_view name) const noexcept;
        bool remove (Structure&);

        template <typename Predicate>
        bool removeIf (Predicate&& pred)
        {
            return soul::removeIf (structs, std::move (pred));
        }

    private:
        ArrayWithPreallocation<StructurePtr, 32> structs;
    };

    //==============================================================================
    bool isProcessor() const;
    bool isGraph() const;
    bool isNamespace() const;

    bool isSystemModule() const;

    Program program;

    std::string shortName;          ///< The unqualified module name without a namespace
    std::string fullName;           ///< The fully-qualified name, with all namespace levels, including the root
    std::string originalFullName;   ///< The fully-qualified name as a user would expect to see it, without a root or other manglings

    std::vector<pool_ref<heart::InputDeclaration>> inputs;
    std::vector<pool_ref<heart::OutputDeclaration>> outputs;

    // Properties if it's a connection graph:
    std::vector<pool_ref<heart::Connection>> connections;
    std::vector<pool_ref<heart::ProcessorInstance>> processorInstances;

    // Properties if it's a processor
    Annotation annotation;
    double sampleRate = 0;
    uint32_t latency = 0;
    CodeLocation location;
    bool moduleUseTestFlag = false;

    //==============================================================================
    heart::Allocator& allocator;

    template <typename Type, typename... Args>
    Type& allocate (Args&&... args)         { return allocator.allocate<Type> (std::forward<Args> (args)...); }

    //==============================================================================
    Functions       functions;
    StateVariables  stateVariables;
    Structs         structs;

    //==============================================================================
    pool_ptr<heart::InputDeclaration>  findInput  (std::string_view name) const;
    pool_ptr<heart::OutputDeclaration> findOutput (std::string_view name) const;

    //==============================================================================
    void rebuildBlockPredecessors();
    void rebuildVariableUseCounts();

private:
    //==============================================================================
    friend class Program;

    uint32_t moduleID = 0;

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

    Module (Program&, ModuleType);
    Module (Program&, const Module& toClone);

    friend class PoolAllocator;

    static Module& createProcessor (Program&);
    static Module& createGraph     (Program&);
    static Module& createNamespace (Program&);
};


} // namespace soul
