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
/** A set of named properties. The value of each item is a Value object. */
struct Annotation
{
    Annotation();
    ~Annotation();
    Annotation (const Annotation&);
    Annotation (Annotation&&);
    Annotation& operator= (const Annotation&);
    Annotation& operator= (Annotation&&);

    struct Property
    {
        std::string name;
        Value value;
    };

    bool isEmpty() const;
    size_t size() const;

    Value getValue (const std::string& name) const;
    Value getValue (const std::string& name, const Value& defaultReturnValue) const;
    bool hasValue (const std::string& name) const;

    bool getBool (const std::string& name, bool defaultValue = false) const;
    double getDouble (const std::string& name, double defaultValue = 0) const;
    int64_t getInt64 (const std::string& name, int64_t defaultValue = 0) const;
    std::string getString (const std::string& name, const std::string& defaultValue = {}) const;

    void set (const std::string& name, int32_t value);
    void set (const std::string& name, int64_t value);
    void set (const std::string& name, float value);
    void set (const std::string& name, double value);
    void set (const std::string& name, bool value);
    void set (const std::string& name, const char* value);
    void set (const std::string& name, const std::string& value);
    void set (const std::string& name, Value newValue, const StringDictionary&);
    void set (const std::string& name, const choc::value::ValueView& value);

    void remove (const std::string& name);

    std::vector<std::string> getNames() const;
    const StringDictionary& getDictionary() const;

    choc::value::Value toExternalValue() const;
    static Annotation fromExternalValue (const choc::value::ValueView&);

    std::string toJSON() const;
    std::string toHEART() const;

private:
    std::unique_ptr<std::vector<Property>> properties;
    StringDictionary dictionary;

    void set (const std::string& name, const void* value) = delete;
    void setInternal (const std::string& name, Value newValue);
};

}
