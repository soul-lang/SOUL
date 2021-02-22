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

/**
    Represents a compiled SOUL program, which is a collection of Modules that have
    been linked together.

    Note that this class is a smart-pointer to a shared, ref-counted underlying object,
    so can be copied by value at no cost. To make a deep copy of a Program,
    use Program::clone().
*/
class Program   final
{
public:
    /** Creates a reference to a new, empty program. */
    Program();
    ~Program();

    Program (const Program&);
    Program (Program&&);
    Program& operator= (const Program&);
    Program& operator= (Program&&);

    /** Returns a deep copy of this program. */
    Program clone() const;

    //==============================================================================
    /** Creates a dump of this program as HEART code.
        @see createFromHEART()
    */
    std::string toHEART() const;

    /** Converts a chunk of HEART code that was emitted by toHEART() back to a Program.
        @see toString()
    */
    static Program createFromHEART (CompileMessageList&, CodeLocation heartCode, bool runSanityCheck);

    //==============================================================================
    /** Return true if the program contains no modules. */
    bool isEmpty() const;

    /** Returns true if the program contains at least one module. */
    operator bool() const;

    /** Provides access to the modules. */
    const std::vector<pool_ref<Module>>& getModules() const;

    /** Removes the given module */
    void removeModule (Module&);

    /** Returns the module that should be used as the main entry point of the program, or nullptr
        if no suitable module exists.
    */
    pool_ptr<Module> findMainProcessor() const;

    /** Returns the main processor, or fails with an error if no suitable module exists. */
    Module& getMainProcessor() const;

    /** Looks for a given module by name. */
    pool_ptr<Module> findModuleWithName (const std::string& name) const;

    /** Looks for a given module by name. */
    Module& getModuleWithName (const std::string& name) const;

    /** Looks for a module that contains the specified function. */
    pool_ptr<Module> findModuleContainingFunction (const heart::Function&) const;
    Module& getModuleContainingFunction (const heart::Function&) const;

    /** Returns the namespace with this name, or creates one if it's not there. */
    Module& getOrCreateNamespace (const std::string& name);

    /** Looks for a variable with a (fully-qualified) name. */
    pool_ptr<heart::Variable> findVariableWithName (const std::string& name) const;

    /** Generates a repeatable hash code for the complete state of this program. */
    std::string getHash() const;

    /** Provides access to the program's string dictionary */
    StringDictionary& getStringDictionary();

    /** Provides access to the program's string dictionary */
    const StringDictionary& getStringDictionary() const;

    /** Provides access to the program's constant table */
    ConstantTable& getConstantTable();

    /** Provides access to the program's constant table */
    const ConstantTable& getConstantTable() const;

    /** Finds a list of all the externals in the program. */
    std::vector<pool_ref<heart::Variable>> getExternalVariables() const;

    /** Returns an ID for one of the modules in the program (which will be unique
        within the program but not globally). The arraySize indicates how many unique ids
        are required for the module, as a range from the returned value
    */
    uint32_t getModuleID (Module&, uint32_t arraySize);

    //==============================================================================
    /** Returns the allocator used to hold all items in the program and its modules. */
    heart::Allocator& getAllocator();

    /** Adds a new graph module at the given index. */
    Module& addGraph (int index = -1);

    /** Adds a new processor module at the given index. */
    Module& addProcessor (int index = -1);

    /** Adds a new namespace module at the given index. */
    Module& addNamespace (int index = -1);

    /** Returns the name of a variable using a fully-qualified name if the variable lies outside the given module. */
    std::string getVariableNameWithQualificationIfNeeded (const Module& context, const heart::Variable&) const;

    /** Returns the fully-qualified path for a variable in a non-mangled format as a user would expect to see it. */
    std::string getExternalVariableName (const heart::Variable&) const;

    /** Returns the name of a function using a fully-qualified name if the function lies outside the given module. */
    std::string getFunctionNameWithQualificationIfNeeded (const Module& context, const heart::Function&) const;

    /** Returns the name of a struct using fully-qualified struct names for structures outside the given module. */
    std::string getStructNameWithQualificationIfNeeded (const Module& context, const Structure&) const;

    /** Returns the name of a struct using fully-qualified struct names for structures outside the given module. */
    std::string getFullyQualifiedStructName (const Structure&) const;

    /** Returns the description of a Type using fully-qualified struct names for structures outside the given module. */
    std::string getTypeDescriptionWithQualificationIfNeeded (pool_ptr<const Module> context, const Type&) const;

    /** Returns the description of a Type using fully-qualified struct names for all structures. */
    std::string getFullyQualifiedTypeDescription (const Type&) const;

    /** Makes a fully-qualified path more readable by removing the internal top-level namespace. */
    static std::string stripRootNamespaceFromQualifiedPath (std::string path);

    /** @internal */
    static const char* getRootNamespaceName();

private:
    //==============================================================================
    friend class Module;
    struct ProgramImpl;
    ProgramImpl* pimpl;
    RefCountedPtr<ProgramImpl> refHolder;

    Program (ProgramImpl&, bool holdRef);
};

} // namespace soul
