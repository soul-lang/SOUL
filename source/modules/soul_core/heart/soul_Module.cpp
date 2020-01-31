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

Module::Module (heart::Allocator& a, ModuleType type) : allocator (a), moduleType (type) {}

Module::Module (heart::Allocator& a, const Module& toClone)
    : moduleName (toClone.moduleName),
      annotation (toClone.annotation),
      allocator (a),
      moduleType (toClone.moduleType)
{
}

Module& Module::createProcessor (heart::Allocator& a)    { return a.allocate<Module> (a, ModuleType::processorModule); }
Module& Module::createGraph     (heart::Allocator& a)    { return a.allocate<Module> (a, ModuleType::graphModule); }
Module& Module::createNamespace (heart::Allocator& a)    { return a.allocate<Module> (a, ModuleType::namespaceModule); }

bool Module::isProcessor() const noexcept       { return moduleType == ModuleType::processorModule; }
bool Module::isGraph() const noexcept           { return moduleType == ModuleType::graphModule; }
bool Module::isNamespace() const noexcept       { return moduleType == ModuleType::namespaceModule; }

std::string Module::getNameWithoutRootNamespace() const         { return Program::stripRootNamespaceFromQualifiedPath (moduleName); }

std::string Module::getNameWithoutRootNamespaceOrSpecialisations() const
{
    return TokenisedPathString (getNameWithoutRootNamespace())
            .withRemovedSections ([] (const std::string& section) { return startsWith (section, "_for"); })
            .fullPath;
}

std::vector<pool_ref<heart::Function>> Module::getExportedFunctions() const
{
    std::vector<pool_ref<heart::Function>> result;

    for (auto& f : functions)
        if (f->isExported)
            result.push_back (f);

    return result;
}

pool_ptr<heart::Function> Module::findRunFunction() const
{
    for (auto& f : functions)
        if (f->functionType.isRun())
            return f;

    return {};
}

heart::Function& Module::getRunFunction() const
{
    return *findRunFunction();
}

pool_ptr<heart::InputDeclaration> Module::findInput (const std::string& name) const
{
    for (auto& f : inputs)
        if (f->name == name)
            return f;

    return {};
}

pool_ptr<heart::OutputDeclaration> Module::findOutput (const std::string& name) const
{
    for (auto& f : outputs)
        if (f->name == name)
            return f;

    return {};
}

heart::Function& Module::getFunction (const std::string& name) const
{
    return *findFunction (name);
}

pool_ptr<heart::Function> Module::findFunction (const std::string& name) const
{
    for (auto& f : functions)
        if (f->name == name)
            return f;

    return {};
}

pool_ptr<heart::Variable> Module::findStateVariable (const std::string& name) const
{
    for (auto& v : stateVariables)
        if (v->name == name)
            return v;

    return {};
}

void Module::addStruct (StructurePtr newStruct)
{
    SOUL_ASSERT (findStruct (newStruct->name) == nullptr); // name clash!
    structs.push_back (newStruct);
}

Structure& Module::addStruct (std::string name)
{
    StructurePtr s (new Structure (name, nullptr));
    addStruct (s);
    return *s;
}

Structure& Module::findOrAddStruct (Identifier name)
{
    if (auto s = findStruct (name))
        return *s;

    return addStruct (name);
}

void Module::rebuildBlockPredecessors()
{
    for (auto& f : functions)
        f->rebuildBlockPredecessors();
}

void Module::rebuildVariableUseCounts()
{
    for (auto& v : stateVariables)
        v->resetUseCount();

    for (auto& f : functions)
        f->rebuildVariableUseCounts();
}

} // namespace soul
