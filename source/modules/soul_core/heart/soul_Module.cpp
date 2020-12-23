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

//==============================================================================
size_t Module::Functions::size() const                                      { return functions.size(); }
pool_ptr<heart::Function> Module::Functions::findRunFunction() const        { return find (heart::getRunFunctionName()); }
heart::Function& Module::Functions::getRunFunction() const                  { return *findRunFunction(); }
ArrayView<pool_ref<heart::Function>> Module::Functions::get() const         { return functions; }
heart::Function& Module::Functions::get (std::string_view name) const       { return *find (name); }
heart::Function& Module::Functions::at (size_t index) const                 { return functions[index]; }
bool Module::Functions::remove (heart::Function& f)                         { return removeItem (functions, f); }
bool Module::Functions::contains (const heart::Function& f) const           { return soul::contains (functions, f); }

pool_ptr<heart::Function> Module::Functions::find (std::string_view name) const
{
    for (auto& f : functions)
        if (f->name == name)
            return f;

    return {};
}

heart::Function& Module::Functions::add (std::string name, bool isEventFunction)
{
    SOUL_ASSERT (find (name) == nullptr);

    auto& fn = module.allocate<heart::Function>();
    fn.name = module.allocator.get (name);

    if (isEventFunction)
    {
        SOUL_ASSERT (! heart::isReservedFunctionName (name));
        fn.functionType = heart::FunctionType::event();
        fn.returnType = PrimitiveType::void_;
    }
    else if (name == heart::getRunFunctionName())         fn.functionType = heart::FunctionType::run();
    else if (name == heart::getUserInitFunctionName())    fn.functionType = heart::FunctionType::userInit();
    else if (name == heart::getSystemInitFunctionName())  fn.functionType = heart::FunctionType::systemInit();

    functions.push_back (fn);
    return fn;
}

heart::Function& Module::Functions::add (const heart::InputDeclaration& input, const Type& type)
{
    SOUL_ASSERT (module.findInput (input.name) == std::addressof (input));
    SOUL_ASSERT (input.isEventEndpoint() && input.canHandleType (type));

    auto functionName = heart::getEventFunctionName (input.name, type);
    SOUL_ASSERT (find (functionName) == nullptr);

    return add (functionName, true);
}

//==============================================================================
size_t Module::StateVariables::size() const                                 { return stateVariables.size(); }
ArrayView<pool_ref<heart::Variable>> Module::StateVariables::get() const    { return stateVariables; }
void Module::StateVariables::clear()                                        { stateVariables.clear(); }

pool_ptr<heart::Variable> Module::StateVariables::find (std::string_view name) const
{
    for (auto& v : stateVariables)
        if (v->name == name)
            return v;

    return {};
}

void Module::StateVariables::add (heart::Variable& v)
{
    SOUL_ASSERT (v.isState() && find (v.name) == nullptr);
    stateVariables.push_back (v);
}

//==============================================================================
size_t Module::Structs::size() const  { return structs.size(); }

ArrayView<StructurePtr> Module::Structs::get() const
{
    return structs;
}

Structure& Module::Structs::add (std::string name)
{
    SOUL_ASSERT (find (name) == nullptr);
    structs.push_back (*new Structure (std::move (name), nullptr));
    return *structs.back();
}

Structure& Module::Structs::add (Structure& s)
{
    SOUL_ASSERT (find (s.getName()) == nullptr);
    structs.push_back (s);
    return s;
}

bool Module::Structs::remove (Structure& s)
{
    return removeItem (structs, s);
}

Structure& Module::Structs::addCopy (Structure& s)
{
    SOUL_ASSERT (find (s.getName()) == nullptr); // name clash!
    structs.push_back (*new Structure (s));
    return *structs.back();
}

StructurePtr Module::Structs::find (std::string_view name) const noexcept
{
    for (auto& s : structs)
        if (name == s->getName())
            return s;

    return {};
}

Structure& Module::Structs::findOrAdd (std::string name)
{
    if (auto s = find (name))
        return *s;

    return add (std::move (name));
}

//==============================================================================
Module::Module (Program& p, ModuleType type)
   : program (*p.pimpl, false), allocator (p.getAllocator()), functions (*this), moduleType (type)
{
}

Module::Module (Program& p, const Module& toClone)
   : program (*p.pimpl, false),
     shortName (toClone.shortName),
     fullName (toClone.fullName),
     originalFullName (toClone.originalFullName),
     annotation (toClone.annotation),
     allocator (p.getAllocator()),
     functions (*this),
     moduleType (toClone.moduleType)
{
}

Module& Module::createProcessor (Program& p)    { return p.getAllocator().allocate<Module> (p, ModuleType::processorModule); }
Module& Module::createGraph     (Program& p)    { return p.getAllocator().allocate<Module> (p, ModuleType::graphModule); }
Module& Module::createNamespace (Program& p)    { return p.getAllocator().allocate<Module> (p, ModuleType::namespaceModule); }

bool Module::isProcessor() const        { return moduleType == ModuleType::processorModule; }
bool Module::isGraph() const            { return moduleType == ModuleType::graphModule; }
bool Module::isNamespace() const        { return moduleType == ModuleType::namespaceModule; }

bool Module::isSystemModule() const     { return choc::text::startsWith (originalFullName, "soul::"); }

pool_ptr<heart::InputDeclaration> Module::findInput (std::string_view name) const
{
    for (auto& f : inputs)
        if (f->name == name)
            return f;

    return {};
}

pool_ptr<heart::OutputDeclaration> Module::findOutput (std::string_view name) const
{
    for (auto& f : outputs)
        if (f->name == name)
            return f;

    return {};
}

void Module::rebuildBlockPredecessors()
{
    for (auto& f : functions.get())
        f->rebuildBlockPredecessors();
}

void Module::rebuildVariableUseCounts()
{
    for (auto& v : stateVariables.get())
        v->readWriteCount.reset();

    for (auto& f : functions.get())
        f->rebuildVariableUseCounts();
}

} // namespace soul
