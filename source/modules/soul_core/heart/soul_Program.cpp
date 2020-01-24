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

#if ! SOUL_INSIDE_CORE_CPP
 #error "Don't add this cpp file to your build, it gets included indirectly by soul_core.cpp"
#endif

namespace soul
{

struct Program::ProgramImpl  : public RefCountedObject
{
    ProgramImpl() = default;
    ProgramImpl (ProgramImpl&&) = delete;
    ProgramImpl (const ProgramImpl&) = delete;

    heart::Allocator allocator;
    std::vector<pool_ref<Module>> modules;
    ConstantTable constantTable;
    StringDictionary stringDictionary;

    int nextModuleId = 1;

    pool_ptr<Module> getModuleWithName (const std::string& name) const
    {
        for (auto& m : modules)
            if (m->moduleName == name || m->getNameWithoutRootNamespace() == name)
                return m;

        return {};
    }

    pool_ptr<Module> getModuleContainingFunction (const heart::Function& f) const
    {
        for (auto& m : modules)
            if (contains (m->functions, f))
                return m;

        return {};
    }

    void removeModule (Module& module)
    {
        removeIf (modules, [&] (pool_ref<Module> m) { return m == module; });
    }

    Module& getOrCreateNamespace (const std::string& name)
    {
        if (auto m = getModuleWithName (name))
            return *m;

        auto newModule = Module::createNamespace (allocator);
        newModule->moduleName = name;
        modules.push_back (newModule);
        return newModule;
    }

    pool_ptr<heart::Variable> getVariableWithName (const std::string& name)
    {
        TokenisedPathString path (name);
        auto parentPath = path.getParentPath();
        auto variableName = path.getLastPart();

        if (auto m = getModuleWithName (TokenisedPathString::join (Program::getRootNamespaceName(), path.getParentPath())))
            return m->findStateVariable (variableName);

        return {};
    }

    pool_ptr<heart::Function> getFunctionWithName (const std::string& name)
    {
        TokenisedPathString path (name);
        auto parentPath = path.getParentPath();
        auto functionName = path.getLastPart();

        if (auto m = getModuleWithName (TokenisedPathString::join (Program::getRootNamespaceName(), path.getParentPath())))
            return m->findFunction (functionName);

        return {};
    }

    pool_ptr<Module> getMainProcessor() const
    {
        for (auto& m : modules)
            if (m->isProcessor() || m->isGraph())
                if (m->annotation.getBool ("main"))
                    return m;

        for (auto& m : modules)
            if (m->isProcessor() || m->isGraph())
                if (! m->annotation.hasValue ("main"))
                    return m;

        return {};
    }

    int getModuleID (Module& m, uint32_t arraySize)
    {
        if (m.moduleId == 0)
        {
            m.moduleId = nextModuleId;
            nextModuleId += static_cast<int> (arraySize);
        }

        return m.moduleId;
    }

    Program clone() const
    {
        Program newProgram;
        newProgram.pimpl->stringDictionary = stringDictionary;

        ModuleCloner::FunctionMappings functionMappings;
        ModuleCloner::StructMappings structMappings;
        std::vector<ModuleCloner> cloners;

        for (auto& m : modules)
        {
            auto& newModule = newProgram.getAllocator().allocate<Module> (newProgram.getAllocator(), m);
            newProgram.pimpl->insert (-1, newModule);
            cloners.emplace_back (m, newModule, functionMappings, structMappings);
        }

        for (auto& c : cloners)
            c.createStructPlaceholders();

        for (auto& c : cloners)
            c.cloneStructAndFunctionPlaceholders();

        for (auto& c : cloners)
            c.clone();

        for (auto& c : constantTable)
            newProgram.pimpl->constantTable.addItem ({ c.handle, std::make_unique<Value> (cloneValue (structMappings, *c.value)) });

        return newProgram;
    }

    std::string getVariableNameWithQualificationIfNeeded (const Module& context, const heart::Variable& v) const
    {
        for (auto& m : modules)
        {
            if (contains (m->stateVariables, v))
            {
                if (m == context)
                    return v.name.toString();

                return stripRootNamespaceFromQualifiedPath (TokenisedPathString::join (m->moduleName, v.name));
            }
        }

        SOUL_ASSERT_FALSE;
        return v.name.toString();
    }

    std::string getFunctionNameWithQualificationIfNeeded (const Module& context, const heart::Function& f) const
    {
        if (auto m = getModuleContainingFunction (f))
        {
            if (m == std::addressof (context))
                return f.name.toString();

            return TokenisedPathString::join (m->moduleName, f.name);
        }

        SOUL_ASSERT_FALSE;
        return f.name.toString();
    }

    std::string getStructNameWithQualificationIfNeeded (pool_ptr<const Module> context, const Structure& s) const
    {
        for (auto& m : modules)
        {
            if (contains (m->structs, &s))
            {
                if (context != nullptr && m == context)
                    return s.name;

                return stripRootNamespaceFromQualifiedPath (TokenisedPathString::join (m->moduleName, s.name));
            }
        }

        SOUL_ASSERT_FALSE;
        return s.name;
    }

    std::string getTypeDescriptionWithQualificationIfNeeded (pool_ptr<const Module> context, const Type& type) const
    {
        if (context == nullptr)
            type.getDescription ([] (const Structure& s) { return s.name; });

        return type.getDescription ([this, context] (const Structure& s) { return getStructNameWithQualificationIfNeeded (context, s); });
    }

    std::string getFullyQualifiedTypeDescription (const Type& type) const
    {
        return type.getDescription ([this] (const Structure& s) { return getStructNameWithQualificationIfNeeded ({}, s); });
    }

    Module& insert (int index, Module& newModule)
    {
        if (index < 0)
            modules.emplace_back (newModule);
        else
            modules.insert (modules.begin() + index, newModule);

        return newModule;
    }

    static Value cloneValue (ModuleCloner::StructMappings& structMappings, const Value& v)
    {
        Value newValue (v);
        newValue.getMutableType() = ModuleCloner::cloneType (structMappings, v.getType());
        return newValue;
    }
};

//==============================================================================
Program::Program() : pimpl (*new ProgramImpl()) {}
Program::Program (ProgramImpl* p) : pimpl (p) {}
Program::~Program() = default;

Program::Program (const Program&) = default;
Program::Program (Program&&) = default;
Program& Program::operator= (const Program&) = default;
Program& Program::operator= (Program&&) = default;

Program Program::createFromHEART (CompileMessageList& messageList, CodeLocation asmCode)
{
    try
    {
        CompileMessageHandler handler (messageList);
        return heart::Parser::parse (std::move (asmCode));
    }
    catch (AbortCompilationException) {}

    return {};
}

Program Program::clone() const                                                          { return pimpl->clone(); }
bool Program::isEmpty() const                                                           { return getModules().empty(); }
Program::operator bool() const                                                          { return ! isEmpty(); }
std::string Program::toHEART() const                                                    { return heart::Printer::getDump (*this); }
const std::vector<pool_ref<Module>>& Program::getModules() const                        { return pimpl->modules; }
void Program::removeModule (Module& module)                                             { return pimpl->removeModule (module); }

pool_ptr<Module> Program::getModuleWithName (const std::string& name) const             { return pimpl->getModuleWithName (name); }
pool_ptr<Module> Program::getModuleContainingFunction (const heart::Function& f) const  { return pimpl->getModuleContainingFunction (f); }
Module& Program::getOrCreateNamespace (const std::string& name)                         { return pimpl->getOrCreateNamespace (name); }
pool_ptr<heart::Function> Program::getFunctionWithName (const std::string& name) const  { return pimpl->getFunctionWithName (name); }
pool_ptr<heart::Variable> Program::getVariableWithName (const std::string& name) const  { return pimpl->getVariableWithName (name); }

heart::Allocator& Program::getAllocator()                                               { return pimpl->allocator; }
Module& Program::addGraph (int index)                                                   { return pimpl->insert (index, Module::createGraph     (pimpl->allocator)); }
Module& Program::addProcessor (int index)                                               { return pimpl->insert (index, Module::createProcessor (pimpl->allocator)); }
Module& Program::addNamespace (int index)                                               { return pimpl->insert (index, Module::createNamespace (pimpl->allocator)); }
pool_ptr<Module> Program::getMainProcessor() const                                      { return pimpl->getMainProcessor(); }
StringDictionary& Program::getStringDictionary()                                        { return pimpl->stringDictionary; }
const StringDictionary& Program::getStringDictionary() const                            { return pimpl->stringDictionary; }
ConstantTable& Program::getConstantTable()                                              { return pimpl->constantTable; }
const ConstantTable& Program::getConstantTable() const                                  { return pimpl->constantTable; }
int Program::getModuleID (Module& m, uint32_t arraySize)                                { return pimpl->getModuleID (m, arraySize); }
const char* Program::getRootNamespaceName()                                             { return "_root"; }
std::string Program::stripRootNamespaceFromQualifiedPath (std::string path)             { return TokenisedPathString::removeTopLevelNameIfPresent (path, getRootNamespaceName()); }

std::string Program::getHash() const
{
    IndentedStream dump;
    heart::Printer::print (*this, dump);
    HashBuilder hash;
    hash << dump.getContent();
    return hash.toString();
}

Module& Program::getMainProcessorOrThrowError() const
{
    auto main = getMainProcessor();

    if (main == nullptr)
        CodeLocation().throwError (Errors::cannotFindMainProcessor());

    SOUL_ASSERT (! main->isNamespace());
    return *main;
}

std::string Program::getVariableNameWithQualificationIfNeeded (const Module& context, const heart::Variable& v) const
{
    return pimpl->getVariableNameWithQualificationIfNeeded (context, v);
}

std::string Program::getFunctionNameWithQualificationIfNeeded (const Module& context, const heart::Function& f) const
{
    return pimpl->getFunctionNameWithQualificationIfNeeded (context, f);
}

std::string Program::getStructNameWithQualificationIfNeeded (const Module& context, const Structure& s) const
{
    return pimpl->getStructNameWithQualificationIfNeeded (context, s);
}

std::string Program::getFullyQualifiedStructName (const Structure& s) const
{
    return pimpl->getStructNameWithQualificationIfNeeded ({}, s);
}

std::string Program::getTypeDescriptionWithQualificationIfNeeded (pool_ptr<const Module> context, const Type& type) const
{
    return pimpl->getTypeDescriptionWithQualificationIfNeeded (context, type);
}

std::string Program::getFullyQualifiedTypeDescription (const Type& type) const
{
    return pimpl->getFullyQualifiedTypeDescription (type);
}

} // namespace soul
