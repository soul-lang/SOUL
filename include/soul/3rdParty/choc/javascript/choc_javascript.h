//
//    ██████ ██   ██  ██████   ██████
//   ██      ██   ██ ██    ██ ██            ** Clean Header-Only Classes **
//   ██      ███████ ██    ██ ██
//   ██      ██   ██ ██    ██ ██           https://github.com/Tracktion/choc
//    ██████ ██   ██  ██████   ██████
//
//   CHOC is (C)2021 Tracktion Corporation, and is offered under the terms of the ISC license:
//
//   Permission to use, copy, modify, and/or distribute this software for any purpose with or
//   without fee is hereby granted, provided that the above copyright notice and this permission
//   notice appear in all copies. THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
//   WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
//   AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
//   CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
//   WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
//   CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#ifndef CHOC_JAVASCRIPT_HEADER_INCLUDED
#define CHOC_JAVASCRIPT_HEADER_INCLUDED

#include "../containers/choc_Value.h"

/**
    A simple javascript engine (currently using duktape internally)
*/
namespace choc::javascript
{
    /// This is thrown by any javascript functions that need to report an error
    struct Error
    {
        std::string message;
    };

    /**
        An execution context which you use for running javascript code.

        Couldn't be much simpler to use one of these: just create a Context, add any
        native bindings that you need using registerFunction(), and then use evaluate()
        or invoke() to execute code with it.

        The context isn't thread safe, so it's up to the caller to handle thread
        synchronisation issues.
    */
    struct Context
    {
        Context();
        ~Context();

        /// Evaluates the given chunk of javascript.
        /// If there are any parse errors, this will throw a choc::javascript::Error exception.
        choc::value::Value evaluate (std::string_view javascriptCode);

        /// Attemps to invoke a global function with a list of arguments.
        /// If there are any parse errors, this will throw a choc::javascript::Error exception.
        template <typename ValueView>
        choc::value::Value invoke (std::string_view functionName, const ValueView* args, size_t numArgs);

        /// This is the prototype for a lambda which can be bound as a javascript function.
        using NativeFunction = std::function<choc::value::Value(const choc::value::Value* args, size_t numArgs)>;

        /// Binds a lambda function to a global name so that javascript code can invoke it.
        void registerFunction (std::string_view name, NativeFunction fn);

    private:
        struct Pimpl;
        std::unique_ptr<Pimpl> pimpl;
    };
}


//==============================================================================
//        _        _           _  _
//     __| |  ___ | |_   __ _ (_)| | ___
//    / _` | / _ \| __| / _` || || |/ __|
//   | (_| ||  __/| |_ | (_| || || |\__ \ _  _  _
//    \__,_| \___| \__| \__,_||_||_||___/(_)(_)(_)
//
//   Code beyond this point is implementation detail...
//
//==============================================================================

/// In order to avoid pulling in the whole of the dependencies, you should set this
/// macro to 1 in one of your compile units
#if CHOC_JAVASCRIPT_IMPLEMENTATION

namespace choc::javascript
{

namespace duktape
{
#if _MSC_VER
 #pragma warning(push)
 #pragma warning(disable : 4018 4127 4244 4505 4611 4702)
#elif __clang__
 #pragma clang diagnostic push
 #pragma clang diagnostic ignored "-Wextra-semi"
 #pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"
 #pragma clang diagnostic ignored "-Wswitch-enum"
 #pragma clang diagnostic ignored "-Wshorten-64-to-32"
 #if __clang_major__ > 10
  #pragma clang diagnostic ignored "-Wc++98-compat-extra-semi"
  #pragma clang diagnostic ignored "-Wimplicit-int-conversion"
  #pragma clang diagnostic ignored "-Wimplicit-float-conversion"
 #else
  #pragma clang diagnostic ignored "-Wconversion"
 #endif
#elif __GNUC__
 #pragma GCC diagnostic push
 #pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
 #pragma GCC diagnostic ignored "-Wsign-conversion"
 #pragma GCC diagnostic ignored "-Wconversion"
 #pragma GCC diagnostic ignored "-Wswitch-enum"
 #pragma GCC diagnostic ignored "-Wunused-variable"
 #pragma GCC diagnostic ignored "-Wredundant-decls"
#endif

 #include "duktape/duktape.c"

#if _MSC_VER
 #pragma warning (pop)
#elif __clang__
 #pragma clang diagnostic pop
#elif __GNUC__
 #pragma GCC diagnostic pop
#endif
}

//==============================================================================
//using namespace duktape;

struct Context::Pimpl
{
    Pimpl() : context (duktape::duk_create_heap (nullptr, nullptr, nullptr, nullptr, fatalError))
    {
        CHOC_ASSERT (context != nullptr);
    }

    ~Pimpl()
    {
        duk_destroy_heap (context);
    }

    void reset()
    {
        while (duk_get_top (context))
            duk_remove (context, duk_get_top_index (context));
    }

    static void fatalError (void*, const char* message)    { throw Error { message }; }

    void throwError()
    {
        std::string message = duk_safe_to_stacktrace (context, -1);
        reset();
        throw Error { message };
    }

    void evalString (std::string_view code)
    {
        if (duk_peval_lstring (context, code.data(), code.length()) != DUK_EXEC_SUCCESS)
            throwError();
    }

    choc::value::Value evaluate (std::string_view javascriptCode)
    {
        evalString (javascriptCode);
        auto result = readValue (context, -1);
        duk_pop (context);
        return result;
    }

    template <typename ValueView>
    choc::value::Value invoke (std::string_view functionName, const ValueView* args, size_t numArgs)
    {
        evalString (functionName);

        if (! duk_is_function (context, -1))
            throw Error { "No such function" };

        duk_require_stack_top (context, (duktape::duk_idx_t) numArgs);

        for (size_t i = 0; i < numArgs; ++i)
            pushValue (context, args[i]);

        if (duk_pcall (context, (duktape::duk_idx_t) numArgs) != DUK_EXEC_SUCCESS)
            throwError();

        auto returnValue = readValue (context, -1);
        duk_pop (context);
        return returnValue;
    }

    void registerFunction (std::string_view name, NativeFunction fn)
    {
        duk_push_global_object (context);

        registeredFunctions.push_back (std::make_unique<RegisteredFunction> (std::move (fn), *this));
        registeredFunctions.back()->pushToStack (context);

        duk_put_prop_lstring (context, -2, name.data(), name.length());
        duk_pop (context);
    }

    static void pushValue (duktape::duk_context* ctx, const choc::value::ValueView& v)
    {
        if (v.isInt() || v.isFloat())   return duk_push_number (ctx, v.get<double>());
        if (v.isBool())                 return duk_push_boolean (ctx, v.getBool());

        if (v.isString())
        {
            auto s = v.getString();
            return (void) duk_push_lstring (ctx, s.data(), s.length());
        }

        if (v.isArray())
        {
            auto arrayIndex = duk_push_array (ctx);
            duktape::duk_uarridx_t elementIndex = 0;

            for (const auto& element : v)
            {
                pushValue (ctx, element);
                duk_put_prop_index (ctx, arrayIndex, elementIndex++);
            }

            return;
        }

        if (v.isObject())
        {
            auto objectIndex = duk_push_object (ctx);

            v.visitObjectMembers ([&] (std::string_view name, const choc::value::ValueView& value)
            {
                pushValue (ctx, value);
                duk_put_prop_lstring (ctx, objectIndex, name.data(), name.length());
            });

            return;
        }

        CHOC_ASSERT (v.isVoid()); // types like vectors aren't currently supported
        return duk_push_undefined (ctx);
    }

    static choc::value::Value readValue (duktape::duk_context* ctx, duktape::duk_idx_t index)
    {
        switch (duk_get_type (ctx, index))
        {
            case DUK_TYPE_NULL:      return {};
            case DUK_TYPE_UNDEFINED: return {};
            case DUK_TYPE_BOOLEAN:   return choc::value::createBool (static_cast<bool> (duk_get_boolean (ctx, index)));
            case DUK_TYPE_NUMBER:    return choc::value::createFloat64 (duk_get_number (ctx, index));

            case DUK_TYPE_STRING:
            {
                duktape::duk_size_t len = 0;

                if (auto s = duk_get_lstring (ctx, index, std::addressof (len)))
                    return choc::value::createString (std::string_view (s, len));

                return choc::value::createString ({});
            }

            case DUK_TYPE_OBJECT:
            case DUK_TYPE_LIGHTFUNC:
            {
                if (duk_is_array (ctx, index))
                {
                    return choc::value::createArray (static_cast<uint32_t> (duk_get_length (ctx, index)),
                                                     [&] (uint32_t i) -> choc::value::Value
                    {
                        duk_get_prop_index (ctx, index, static_cast<duktape::duk_uarridx_t> (i));
                        auto element = readValue (ctx, -1);
                        duk_pop (ctx);
                        return element;
                    });
                }

                if (duk_is_function (ctx, index) || duk_is_lightfunc (ctx, index))
                {
                    CHOC_ASSERT (false); // we don't currently handle function objects
                    return {};
                }

                // Handle a plain object
                auto object = choc::value::createObject ("object");

                for (duk_enum (ctx, index, DUK_ENUM_OWN_PROPERTIES_ONLY);
                     duk_next (ctx, -1, 1);
                     duk_pop_2 (ctx))
                {
                    object.addMember (duk_to_string (ctx, -2), readValue (ctx, -1));
                }

                duk_pop (ctx);
                return object;
            }

            case DUK_TYPE_NONE:
            default:
                CHOC_ASSERT (false);
                return {};
        }
    }

    struct RegisteredFunction
    {
        RegisteredFunction (NativeFunction&& f, Pimpl& p) : function (std::move (f)), owner (p) {}

        NativeFunction function;
        Pimpl& owner;

        static duktape::duk_ret_t invoke (duktape::duk_context* ctx)
        {
            duk_push_current_function (ctx);
            auto& fn = RegisteredFunction::get (ctx, -1);
            duk_pop (ctx);

            auto numArgs = duk_get_top (ctx);

            std::vector<choc::value::Value> args;
            args.reserve ((size_t) numArgs);

            for (decltype (numArgs) i = 0; i < numArgs; ++i)
                args.push_back (readValue (ctx, i));

            auto result = std::invoke (fn.function, args.data(), numArgs);

            if (result.isVoid())
                return 0;

            pushValue (ctx, result);
            return 1;
        }

        static duktape::duk_ret_t destroy (duktape::duk_context* ctx)
        {
            duk_require_function (ctx, 0);
            RegisteredFunction::get (ctx, 0).deregister();
            duk_pop (ctx);
            return 0;
        }

        void attachToObject (duktape::duk_context* ctx)
        {
            duk_push_pointer (ctx, (void*) this);
            duk_put_prop_string (ctx, -2, DUK_HIDDEN_SYMBOL("registeredFn"));
        }

        void pushToStack (duktape::duk_context* ctx)
        {
            duk_push_c_function (ctx, RegisteredFunction::invoke, (duktape::duk_int_t) (-1));
            attachToObject (ctx);
            duk_push_c_function (ctx, RegisteredFunction::destroy, 1);
            attachToObject (ctx);
            duk_set_finalizer (ctx, -2);
        }

        void deregister()
        {
            auto oldEnd = owner.registeredFunctions.end();
            auto newEnd = std::remove_if (owner.registeredFunctions.begin(), oldEnd,
                                          [this] (auto& f) { return f.get() == this; });

            if (newEnd != oldEnd)
                owner.registeredFunctions.erase (newEnd, oldEnd);
        }

        static RegisteredFunction& get (duktape::duk_context* ctx, duktape::duk_idx_t index)
        {
            duk_get_prop_string (ctx, index, DUK_HIDDEN_SYMBOL("registeredFn"));
            auto r = static_cast<RegisteredFunction*> (duk_get_pointer (ctx, -1));
            CHOC_ASSERT (r != nullptr);
            duk_pop (ctx);
            return *r;
        }
    };

    duktape::duk_context* context = nullptr;
    std::vector<std::unique_ptr<RegisteredFunction>> registeredFunctions;
};

inline Context::Context()  : pimpl (std::make_unique<Pimpl>()) {}
inline Context::~Context() = default;

inline choc::value::Value Context::evaluate (std::string_view javascriptCode)
{
    return pimpl->evaluate (std::move (javascriptCode));
}

template <typename ValueView>
choc::value::Value Context::invoke (std::string_view functionName, const ValueView* args, size_t numArgs)
{
    return pimpl->invoke (functionName, args, numArgs);
}

inline void Context::registerFunction (std::string_view name, NativeFunction fn)
{
    pimpl->registerFunction (std::move (name), std::move (fn));
}

}

#endif // CHOC_JAVASCRIPT_IMPLEMENTATION
#endif // CHOC_JAVASCRIPT_HEADER_INCLUDED
