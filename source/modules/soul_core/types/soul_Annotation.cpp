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

Annotation::Annotation() = default;
Annotation::~Annotation() = default;
Annotation::Annotation (Annotation&&) = default;
Annotation& Annotation::operator= (Annotation&&) = default;

Annotation::Annotation (const Annotation& other)
{
    if (other.properties != nullptr)
        properties = std::make_unique<std::vector<Property>> (*other.properties);
}

Annotation& Annotation::operator= (const Annotation& other)
{
    if (other.properties != nullptr)
        properties = std::make_unique<std::vector<Property>> (*other.properties);

    return *this;
}

static std::string propertyToString (const StringDictionary& stringDictionary, const Annotation::Property& prop, bool asJSON)
{
    auto desc = ((asJSON || ! isSafeIdentifierName (prop.name)) ? addDoubleQuotes (prop.name) : prop.name) + ": ";

    const auto& type = prop.value.getType();
    SOUL_ASSERT (type.isPrimitive() || type.isStringLiteral());

    if (type.isStringLiteral())
        return desc + addDoubleQuotes (stringDictionary.getStringForHandle (prop.value.getStringLiteral()));

    if (asJSON)
    {
        if (type.isPrimitiveFloat())     return desc + doubleToJSONString (prop.value.getAsDouble());
        if (type.isPrimitiveInteger())   return desc + std::to_string (prop.value.getAsInt64());
    }

    return desc + prop.value.getDescription();
}

static std::string annotationToString (const StringDictionary& stringDictionary, const std::vector<Annotation::Property>* properties, bool asJSON)
{
    if (properties == nullptr || properties->empty())
        return {};

    std::vector<std::string> propertyStrings;

    for (auto& p : *properties)
        propertyStrings.push_back (propertyToString (stringDictionary, p, asJSON));

    auto content = joinStrings (propertyStrings, ", ");

    return asJSON ? ("{ "   + content + " }")
                  : (" [[ " + content + " ]]");
}

bool Annotation::isEmpty() const    { return size() == 0; }
size_t Annotation::size() const     { return properties == nullptr ? 0 : properties->size(); }

Value Annotation::getValue (const std::string& name) const
{
    SOUL_ASSERT (! name.empty());
    return getValue (name, {});
}

Value Annotation::getValue (const std::string& name, const Value& defaultReturnValue) const
{
    SOUL_ASSERT (! name.empty());

    if (properties != nullptr)
        for (auto& p : *properties)
            if (p.name == name)
                return p.value;

    return defaultReturnValue;
}

bool Annotation::hasValue (const std::string& name) const
{
    SOUL_ASSERT (! name.empty());

    if (properties != nullptr)
        for (auto& p : *properties)
            if (p.name == name)
                return true;

    return false;
}

bool Annotation::getBool (const std::string& name, bool defaultValue) const
{
    auto v = getValue (name);
    return v.isValid() ? v.getAsBool() : defaultValue;
}

double Annotation::getDouble (const std::string& name, double defaultValue) const
{
    auto v = getValue (name);
    return v.getType().isPrimitiveFloat() || v.getType().isPrimitiveInteger() ? v.getAsDouble() : defaultValue;
}

int64_t Annotation::getInt64 (const std::string& name, int64_t defaultValue) const
{
    auto v = getValue (name);
    return v.getType().isPrimitiveFloat() || v.getType().isPrimitiveInteger() ? v.getAsInt64() : defaultValue;
}

void Annotation::setStringLiteral (const std::string& name, StringDictionary::Handle stringHandle)
{
    set (name, soul::Value::createStringLiteral (stringHandle));
}

StringDictionary::Handle Annotation::getStringLiteral (const std::string& name) const
{
    auto value = getValue (name);

    if (value.getType().isStringLiteral())
        return value.getStringLiteral();

    return {};
}

void Annotation::set (const std::string& name, Value newValue)
{
    SOUL_ASSERT (! name.empty());

    if (properties == nullptr)
    {
        properties = std::make_unique<std::vector<Property>>();
    }
    else
    {
        for (auto& p : *properties)
        {
            if (p.name == name)
            {
                p.value = std::move (newValue);
                return;
            }
        }
    }

    properties->push_back ({ name, std::move (newValue) });
}

void Annotation::remove (const std::string& name)
{
    SOUL_ASSERT (! name.empty());

    if (properties != nullptr)
        removeIf (*properties, [&] (const Property& p) { return p.name == name; });
}

std::vector<std::string> Annotation::getNames() const
{
    std::vector<std::string> result;

    if (properties != nullptr)
    {
        result.reserve (properties->size());

        for (auto& p : *properties)
            result.push_back (p.name);
    }

    return result;
}

std::string Annotation::toJSON  (const StringDictionary& stringDictionary) const   { return annotationToString (stringDictionary, properties.get(), true); }
std::string Annotation::toHEART (const StringDictionary& stringDictionary) const   { return annotationToString (stringDictionary, properties.get(), false); }

}
