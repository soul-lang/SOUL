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

Annotation::Annotation (const Annotation& other)  : dictionary (other.dictionary)
{
    if (other.properties != nullptr)
        properties = std::make_unique<std::vector<Property>> (*other.properties);
}

Annotation& Annotation::operator= (const Annotation& other)
{
    if (other.properties != nullptr)
        properties = std::make_unique<std::vector<Property>> (*other.properties);

    dictionary = other.dictionary;
    return *this;
}

static std::string propertyToString (const StringDictionary& stringDictionary, const Annotation::Property& prop, bool asJSON)
{
    auto desc = ((asJSON || ! isSafeIdentifierName (prop.name)) ? choc::text::addDoubleQuotes (prop.name) : prop.name) + ": ";

    const auto& type = prop.value.getType();
    SOUL_ASSERT (type.isPrimitive() || type.isStringLiteral());

    if (type.isStringLiteral())
        return desc + choc::json::getEscapedQuotedString (stringDictionary.getStringForHandle (prop.value.getStringLiteral()));

    if (asJSON)
    {
        if (type.isPrimitiveFloat())     return desc + choc::json::doubleToString (prop.value.getAsDouble());
        if (type.isPrimitiveInteger())   return desc + std::to_string (prop.value.getAsInt64());
    }

    return desc + prop.value.getDescription();
}

static std::string annotationToString (const StringDictionary& stringDictionary, const std::vector<Annotation::Property>* properties, bool asJSON)
{
    if (properties == nullptr || properties->empty())
        return {};

    auto content = joinStrings (*properties, ", ", [&] (auto& p) { return propertyToString (stringDictionary, p, asJSON); });

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

std::string Annotation::getString (const std::string& name, const std::string& defaultValue) const
{
    auto v = getValue (name);

    if (v.isValid())
    {
        struct UnquotedPrinter  : public ValuePrinter
        {
            std::ostringstream out;

            void print (std::string_view s) override                        { out << s; }
            void printStringLiteral (StringDictionary::Handle h) override   { print (dictionary->getStringForHandle (h)); }
        };

        UnquotedPrinter p;
        p.dictionary = std::addressof (dictionary);
        v.print (p);
        return p.out.str();
    }

    return defaultValue;
}

static void replaceStringLiterals (Value& v, SubElementPath path, const StringDictionary& sourceDictionary, StringDictionary& destDictionary)
{
    auto value = v.getSubElement (path);

    if (value.getType().isStringLiteral())
    {
        auto s = sourceDictionary.getStringForHandle (value.getStringLiteral());
        v.modifySubElementInPlace (path, Value::createStringLiteral (destDictionary.getHandleForString (s)));
    }
    else if (value.getType().isFixedSizeArray())
    {
        for (size_t i = 0; i < value.getType().getArraySize(); ++i)
             replaceStringLiterals (v, path + i, sourceDictionary, destDictionary);
    }
    else if (value.getType().isStruct())
    {
        for (size_t i = 0; i < value.getType().getStructRef().getNumMembers(); ++i)
             replaceStringLiterals (v, path + i, sourceDictionary, destDictionary);
    }
}

void Annotation::setInternal (const std::string& name, Value newValue)
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

void Annotation::set (const std::string& name, Value newValue, const StringDictionary& sourceDictionary)
{
    replaceStringLiterals (newValue, {}, sourceDictionary, dictionary);
    setInternal (name, std::move (newValue));
}

void Annotation::set (const std::string& name, int32_t value)             { setInternal (name, Value::createInt32 (value)); }
void Annotation::set (const std::string& name, int64_t value)             { setInternal (name, Value::createInt64 (value)); }
void Annotation::set (const std::string& name, float value)               { setInternal (name, Value (value)); }
void Annotation::set (const std::string& name, double value)              { setInternal (name, Value (value)); }
void Annotation::set (const std::string& name, bool value)                { setInternal (name, Value (value)); }
void Annotation::set (const std::string& name, const char* value)         { set (name, std::string (value)); }
void Annotation::set (const std::string& name, const std::string& value)  { setInternal (name, Value::createStringLiteral (dictionary.getHandleForString (value))); }

void Annotation::set (const std::string& name, const choc::value::ValueView& value)
{
    if (value.isInt32())    return set (std::string (name), value.getInt32());
    if (value.isInt64())    return set (std::string (name), value.getInt64());
    if (value.isFloat32())  return set (std::string (name), value.getFloat32());
    if (value.isFloat64())  return set (std::string (name), value.getFloat64());
    if (value.isBool())     return set (std::string (name), value.getBool());
    if (value.isString())   return set (std::string (name), std::string (value.getString()));

    SOUL_ASSERT_FALSE; // other types not currently handled..
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

const StringDictionary& Annotation::getDictionary() const     { return dictionary; }

choc::value::Value Annotation::toExternalValue() const
{
    auto o = choc::value::createObject ("Annotation");

    if (properties != nullptr)
        for (auto& p : *properties)
            o.addMember (p.name, p.value.toExternalValue (ConstantTable(), dictionary));

    return o;
}

Annotation Annotation::fromExternalValue (const choc::value::ValueView& v)
{
    Annotation a;

    if (v.isObjectWithClassName ("Annotation"))
    {
        v.visitObjectMembers ([&] (std::string_view name, const choc::value::ValueView& value)
                              {
                                  a.set (std::string (name), value);
                              });
    }

    return a;
}

std::string Annotation::toJSON() const    { return annotationToString (dictionary, properties.get(), true); }
std::string Annotation::toHEART() const   { return annotationToString (dictionary, properties.get(), false); }

}
