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

Module::Module (Program& p, ModuleType type) : program (*p.pimpl, false), allocator (p.getAllocator()), moduleType (type)
{
}

Module::Module (Program& p, const Module& toClone)
    : program (*p.pimpl, false),
      shortName (toClone.shortName),
      fullName (toClone.fullName),
      originalFullName (toClone.originalFullName),
      annotation (toClone.annotation),
      allocator (p.getAllocator()),
      moduleType (toClone.moduleType)
{
}

Module& Module::createProcessor (Program& p)    { return p.getAllocator().allocate<Module> (p, ModuleType::processorModule); }
Module& Module::createGraph     (Program& p)    { return p.getAllocator().allocate<Module> (p, ModuleType::graphModule); }
Module& Module::createNamespace (Program& p)    { return p.getAllocator().allocate<Module> (p, ModuleType::namespaceModule); }

bool Module::isProcessor() const        { return moduleType == ModuleType::processorModule; }
bool Module::isGraph() const            { return moduleType == ModuleType::graphModule; }
bool Module::isNamespace() const        { return moduleType == ModuleType::namespaceModule; }

bool Module::isSystemModule() const     { return startsWith (originalFullName, "soul::"); }

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

Structure& Module::addStruct (std::string name)
{
    SOUL_ASSERT (findStruct (name) == nullptr); // name clash!
    structs.push_back (*new Structure (std::move (name), nullptr));
    return *structs.back();
}

Structure& Module::addStructCopy (Structure& s)
{
    SOUL_ASSERT (findStruct (s.getName()) == nullptr); // name clash!
    structs.push_back (*new Structure (s));
    return *structs.back();
}

Structure& Module::findOrAddStruct (std::string name)
{
    if (auto s = findStruct (name))
        return *s;

    return addStruct (std::move (name));
}

void Module::rebuildBlockPredecessors()
{
    for (auto& f : functions)
        f->rebuildBlockPredecessors();
}

void Module::rebuildVariableUseCounts()
{
    for (auto& v : stateVariables)
        v->readWriteCount.reset();

    for (auto& f : functions)
        f->rebuildVariableUseCounts();
}

} // namespace soul
