/*
     _____ _____ _____ __
    |   __|     |  |  |  |
    |__   |  |  |  |  |  |__
    |_____|_____|_____|_____|

    Copyright (c) 2018 - ROLI Ltd.
*/

/*
    This file contains various global definitions for data types that are
    needed in various parts of the codebase, both API and the libraries.
*/

#ifndef SOUL_VENUE_MESSAGES_HEADER_INCLUDED
#define SOUL_VENUE_MESSAGES_HEADER_INCLUDED

#include <string>
#include <vector>
#include "../../3rdParty/choc/containers/choc_Value.h"

namespace soul
{

//==============================================================================
enum class SessionState
{
    unknown,
    empty,
    loading,
    loaded,
    linking,
    linked,
    running
};

static constexpr const char* sessionStateToString (SessionState state)
{
    switch (state)
    {
        case SessionState::empty:      return "empty";
        case SessionState::loading:    return "loading";
        case SessionState::loaded:     return "loaded";
        case SessionState::linking:    return "linking";
        case SessionState::linked:     return "linked";
        case SessionState::running:    return "running";

        case SessionState::unknown:
        default:                       return nullptr;
    }
}

static inline SessionState stringToSessionState (std::string_view state)
{
    if (state == "empty")     return SessionState::empty;
    if (state == "loading")   return SessionState::loading;
    if (state == "loaded")    return SessionState::loaded;
    if (state == "linking")   return SessionState::linking;
    if (state == "linked")    return SessionState::linked;
    if (state == "running")   return SessionState::running;

    return SessionState::unknown;
}

enum class EndpointType
{
    unknown,
    stream,
    value,
    event
};

static constexpr const char* endpointTypeToString (EndpointType type)
{
    switch (type)
    {
        case EndpointType::stream:    return "stream";
        case EndpointType::value:     return "value";
        case EndpointType::event:     return "event";
        case EndpointType::unknown:
        default:                      return nullptr;
    }
}

static inline EndpointType stringToEndpointType (std::string_view type)
{
    if (type == "stream")   return EndpointType::stream;
    if (type == "value")    return EndpointType::value;
    if (type == "event")    return EndpointType::event;

    return EndpointType::unknown;
}

struct Endpoint
{
    std::string ID, name;
    EndpointType type;
    std::vector<choc::value::Type> dataTypes;
    choc::value::Value annotation;
};

//==============================================================================
/**
    Contains a complete set of all the sources and settings needed to compile and
    link a program.
*/
struct BuildBundle
{
    struct SourceFile
    {
        std::string filename, content;
    };

    struct Settings
    {
        double       sampleRate         = 0;
        uint32_t     maxBlockSize       = 0;
        size_t       maxStateSize       = 0;
        int          optimisationLevel  = -1;
        int32_t      sessionID          = 0;
        std::string  mainProcessor;
    };

    std::vector<SourceFile> sourceFiles;
    Settings settings;
    choc::value::Value customSettings;
};


} // namespace soul

#endif // SOUL_VENUE_MESSAGES_HEADER_INCLUDED
