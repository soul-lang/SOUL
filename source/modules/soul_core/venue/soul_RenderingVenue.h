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
/**
    An implementation of a Venue which implements most of the functionality needed
    to run asynchronously, with a caller just needing to pump its render callback.
*/
class RenderingVenue  : public Venue
{
public:
    RenderingVenue (std::unique_ptr<PerformerFactory>);
    ~RenderingVenue() override;

    /** This method needs to be called by either a thread or an audio callback
        to keep the rendering process running.
        @returns either nullptr if all went well, or an error message.
    */
    const char* render (uint32_t numFrames);

    //==============================================================================
    bool createSession (SessionReadyCallback) override;

    choc::span<const EndpointDetails> getExternalInputEndpoints() override;
    choc::span<const EndpointDetails> getExternalOutputEndpoints() override;

private:
    struct Pimpl;
    std::unique_ptr<Pimpl> pimpl;
};


} // namespace soul
