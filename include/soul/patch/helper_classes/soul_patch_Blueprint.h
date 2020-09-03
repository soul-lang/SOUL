/*
     _____ _____ _____ __
    |   __|     |  |  |  |
    |__   |  |  |  |  |  |__
    |_____|_____|_____|_____|

    Copyright (c) 2018 - ROLI Ltd.
*/

#pragma once

#include "soul_patch_AudioPluginFormat.h"

#if JUCE_MODULE_AVAILABLE_blueprint

namespace soul
{
namespace patch
{

//==============================================================================
/**
    If you include the juce_blueprint module in your project, this utility class can
    be used to add support for blueprint editors.

    To use it, you can attach it to a SOULPatchAudioProcessor like this:

    @code
    processor->createCustomGUI = soul::patch::BlueprintEditorComponent::getCreateFunction();
    @endcode

    or pass the result of soul::patch::BlueprintEditorComponent::getCreateFunction() to a
    SOULPatchAudioPluginFormat via its constructor.
*/
struct BlueprintEditorComponent  : public juce::AudioProcessorEditor,
                                   private juce::Timer
{
    BlueprintEditorComponent (soul::patch::SOULPatchAudioProcessor& p, const soul::patch::VirtualFile::Ptr& v)
        : juce::AudioProcessorEditor (p), patch (p), view (v)
    {
        initialiseParameterList();
        registerBindings();

        addAndMakeVisible (reactRootComponent);
        setSize (300, 150);
        setResizeLimits (200, 100, 4000, 4000);

        auto fileToLoad = juce::File::getCurrentWorkingDirectory()
                            .getChildFile (soul::patch::String::Ptr (view->getAbsolutePath()));

        reactRootComponent.afterBundleEval = [fileToLoad, this] (const juce::File& bundle)
        {
            if (bundle == fileToLoad)
                this->bundleLoaded();
        };

        reactRootComponent.evaluate (fileToLoad);
        reactRootComponent.enableHotReloading();
    }

    ~BlueprintEditorComponent() override
    {
        patch.editorBeingDeleted (this);
        setLookAndFeel (nullptr);
    }

    static juce::AudioProcessorEditor* createIfAvailable (soul::patch::SOULPatchAudioProcessor& p)
    {
        for (auto& view : p.findViewFiles())
        {
            auto name = soul::patch::String::Ptr (view->getName()).toString<juce::String>();

            if (name.endsWithIgnoreCase (".js") && view->getSize() != 0)
                return new BlueprintEditorComponent (p, view);
        }

        return {};
    }

    static SOULPatchAudioProcessor::CreatePatchGUIEditorFn getCreateFunction()
    {
        return [] (auto& sp) { return createIfAvailable (sp); };
    }

    void resized() override
    {
        reactRootComponent.setBounds (getLocalBounds());
    }

private:
    void bundleLoaded()
    {
        startTimerHz (30);
    }

    template <int numParams, typename MethodType>
    juce::var call (MethodType method, const juce::var::NativeFunctionArgs& args)
    {
        if (args.numArguments != numParams)
            return juce::var::undefined();

        if constexpr (numParams == 0)    return (this->*method)();
        if constexpr (numParams == 1)    return (this->*method) (args.arguments[0]);
        if constexpr (numParams == 2)    return (this->*method) (args.arguments[0], args.arguments[1]);
        if constexpr (numParams == 3)    return (this->*method) (args.arguments[0], args.arguments[1], args.arguments[2]);
        if constexpr (numParams == 4)    return (this->*method) (args.arguments[0], args.arguments[1], args.arguments[2], args.arguments[3]);
        static_assert (numParams <= 4);
        return {};
    }

    template <int numParams, typename MethodType>
    void addMethodBinding (const char* name, MethodType method)
    {
        reactRootComponent.engine.registerNativeMethod (name,
            [this, method] (const juce::var::NativeFunctionArgs& args) -> juce::var
            {
                return call<numParams> (method, args);
            });
    }

    void registerBindings()
    {
        addMethodBinding<1> ("print",                         &BlueprintEditorComponent::print);
        addMethodBinding<0> ("getPatchDescription",           &BlueprintEditorComponent::getPatchDescription);
        addMethodBinding<0> ("getManifest",                   &BlueprintEditorComponent::getManifest);
        addMethodBinding<0> ("getAllParameterIDs",            &BlueprintEditorComponent::getAllParameterIDs);
        addMethodBinding<0> ("getInputEventEndpointIDs",      &BlueprintEditorComponent::getInputEventEndpointIDs);
        addMethodBinding<0> ("getOutputEventEndpointIDs",     &BlueprintEditorComponent::getOutputEventEndpointIDs);
        addMethodBinding<1> ("getEndpointDetails",            &BlueprintEditorComponent::getEndpointDetails);
        addMethodBinding<1> ("beginParameterChangeGesture",   &BlueprintEditorComponent::beginParameterChangeGesture);
        addMethodBinding<1> ("endParameterChangeGesture",     &BlueprintEditorComponent::endParameterChangeGesture);
        addMethodBinding<2> ("setParameterValue",             &BlueprintEditorComponent::setParameterValue);
        addMethodBinding<1> ("getParameterValue",             &BlueprintEditorComponent::getParameterValue);
        addMethodBinding<1> ("getParameterState",             &BlueprintEditorComponent::getParameterState);
    }

    void initialiseParameterList()
    {
        auto params = patch.getPatchParameters();

        for (auto& p : params)
            parameterIDMap[p->paramID] = p;

        size_t i = 0;

        for (auto handle : dirtyParameterList.initialise (params))
        {
            params[i++]->valueChangedCallback = [handle, dirtyList = std::addressof (dirtyParameterList)] (float) { dirtyList->markAsDirty (handle); };
            dirtyParameterList.markAsDirty (handle);
        }
    }

    juce::var print (const juce::var& v)
    {
        std::cout << (v.isString() ? v.toString() : juce::JSON::toString (v, false)) << std::endl;
        return juce::var::undefined();
    }

    juce::var getPatchDescription() const
    {
        auto desc = soul::patch::Description::Ptr (patch.getPatchInstance().getDescription());

        auto* object = new juce::DynamicObject();
        juce::var v (object);

        object->setProperty (ids.UID, desc->UID);
        object->setProperty (ids.version, desc->version);
        object->setProperty (ids.name, desc->name);
        object->setProperty (ids.description, desc->description);
        object->setProperty (ids.category, desc->category);
        object->setProperty (ids.manufacturer, desc->manufacturer);
        object->setProperty (ids.URL, desc->URL);
        object->setProperty (ids.isInstrument, desc->isInstrument);

        return v;
    }

    juce::var getManifest() const
    {
        auto desc = soul::patch::Description::Ptr (patch.getPatchInstance().getDescription());

        if (desc->manifestFile != nullptr)
        {
            std::string errorMessage;
            auto content = soul::patch::loadVirtualFileAsString (*desc->manifestFile, errorMessage);

            if (! errorMessage.empty())
                return juce::String (errorMessage);

            juce::var json;
            auto result = juce::JSON::parse (content, json);

            if (result.failed())
                return result.getErrorMessage();

            return json;
        }

        return juce::var::undefined();
    }

    template <typename ArrayType>
    static juce::var getEndpointIDs (const ArrayType& endpoints)
    {
        juce::StringArray endointsIDs;

        for (auto& e : endpoints)
            endointsIDs.add (e.ID.template toString<juce::String>());

        return endointsIDs;
    }

    juce::var getInputEventEndpointIDs()
    {
        if (auto player = patch.getPatchPlayer())
            return getEndpointIDs (player->getInputEventEndpoints());

        return {};
    }

    juce::var getOutputEventEndpointIDs()
    {
        if (auto player = patch.getPatchPlayer())
            return getEndpointIDs (player->getOutputEventEndpoints());

        return {};
    }

    juce::var getEndpointDetails (const juce::String& endpointID)
    {
        auto* object = new juce::DynamicObject();
        juce::var v (object);

        if (auto player = patch.getPatchPlayer())
        {
            auto details = player->getEndpointDetails (endpointID.toRawUTF8());

            if (details.type != soul::EndpointType::unknown)
            {
                object->setProperty (ids.ID,   details.ID.toString<juce::String>());
                object->setProperty (ids.name, details.name.toString<juce::String>());
                object->setProperty (ids.type, soul::endpointTypeToString (details.type));

                auto annotation = details.annotation.get();

                if (annotation.isObject())
                    object->setProperty (ids.annotation, juce::JSON::parse (choc::json::toString (annotation)));

                juce::var types;

                for (uint32_t i = 0; i < details.numValueTypes; ++i)
                    types.append (juce::var (soul::dump (details.valueTypes[i].get())));

                object->setProperty (ids.valueTypes, types);
            }
        }

        return v;
    }

    juce::var getAllParameterIDs()
    {
        juce::StringArray paramIDs;

        for (auto& p : patch.getPatchParameters())
            paramIDs.add (p->paramID);

        return paramIDs;
    }

    soul::patch::SOULPatchAudioProcessor::PatchParameter* getParameterForID (const juce::String& paramID) const
    {
        auto param = parameterIDMap.find (paramID);

        if (param != parameterIDMap.end())
            return param->second;

        return {};
    }

    juce::var beginParameterChangeGesture (const juce::String& paramID)
    {
        if (auto param = getParameterForID (paramID))
            param->beginChangeGesture();

        return juce::var::undefined();
    }

    juce::var endParameterChangeGesture (const juce::String& paramID)
    {
        if (auto param = getParameterForID (paramID))
            param->endChangeGesture();

        return juce::var::undefined();
    }

    juce::var setParameterValue (const juce::String& paramID, float value)
    {
        if (auto param = getParameterForID (paramID))
            param->setFullRangeValueNotifyingHost (value);

        return juce::var::undefined();
    }

    juce::var getParameterState (const juce::String& paramID)
    {
        if (auto param = getParameterForID (paramID))
        {
            auto* object = new juce::DynamicObject();
            juce::var v (object);

            object->setProperty (ids.ID,          param->paramID);
            object->setProperty (ids.name,        param->name);
            object->setProperty (ids.value,       param->param->getValue());
            object->setProperty (ids.min,         param->param->minValue);
            object->setProperty (ids.max,         param->param->maxValue);
            object->setProperty (ids.step,        param->param->step);
            object->setProperty (ids.init,        param->param->initialValue);
            object->setProperty (ids.textValues,  param->textValues);
            object->setProperty (ids.isBool,      param->isBool);

            return v;
        }

        return juce::var::undefined();
    }

    juce::var getParameterValue (const juce::String& paramID)
    {
        if (auto param = getParameterForID (paramID))
        {
            auto* object = new juce::DynamicObject();
            juce::var v (object);

            auto value = param->param->getValue();

            object->setProperty (ids.value,        value);
            object->setProperty (ids.stringValue,  param->getTextForFullRangeValue (value, 0));

            return v;
        }

        return juce::var::undefined();
    }

    void timerCallback() override
    {
        for (int i = 100; --i >= 0;)
        {
            if (auto param = dirtyParameterList.popNextDirtyObject())
                reactRootComponent.dispatchEvent ("parameterValueChange", param->paramID);
            else
                break;
        }
    }

    soul::patch::SOULPatchAudioProcessor& patch;
    soul::patch::VirtualFile::Ptr view;
    blueprint::ReactApplicationRoot reactRootComponent;

    std::unordered_map<juce::String, soul::patch::SOULPatchAudioProcessor::PatchParameter*> parameterIDMap;
    choc::fifo::DirtyList<soul::patch::SOULPatchAudioProcessor::PatchParameter> dirtyParameterList;

    struct IDs
    {
        const juce::Identifier ID           { "ID" },
                               UID          { "UID" },
                               name         { "name" },
                               description  { "description" },
                               version      { "version" },
                               URL          { "URL" },
                               value        { "value" },
                               min          { "min" },
                               max          { "max" },
                               step         { "step" },
                               init         { "init" },
                               unit         { "unit" },
                               type         { "type" },
                               valueTypes   { "valueTypes" },
                               annotation   { "annotation" },
                               textValues   { "textValues" },
                               isBool       { "isBool" },
                               isInstrument { "isInstrument" },
                               category     { "category" },
                               manufacturer { "manufacturer" },
                               stringValue  { "stringValue" };
    };

    IDs ids;
};

} // namespace patch
} // namespace soul

#endif // JUCE_MODULE_AVAILABLE_blueprint
