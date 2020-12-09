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

bool inExceptionHandler()
{
   #if defined(__APPLE__) && MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_12
    return std::uncaught_exception();
   #else
    return std::uncaught_exceptions() != 0;
   #endif
}

ScopedDisableDenormals::ScopedDisableDenormals() noexcept  : oldFlags (getFPMode())
{
   #if SOUL_ARM64 || SOUL_ARM32
    constexpr intptr_t mask = 1 << 24;
   #else
    constexpr intptr_t mask = 0x8040;
   #endif

    setFPMode (oldFlags | mask);
}

ScopedDisableDenormals::~ScopedDisableDenormals() noexcept
{
    setFPMode (oldFlags);
}

intptr_t ScopedDisableDenormals::getFPMode() noexcept
{
    intptr_t flags = 0;

   #if SOUL_ARM64
    asm volatile("mrs %0, fpcr" : "=r" (flags));
   #elif SOUL_ARM32
    asm volatile("vmrs %0, fpscr" : "=r" (flags));
   #elif SOUL_INTEL
    flags = static_cast<intptr_t> (_mm_getcsr());
   #endif

    return flags;
}

void ScopedDisableDenormals::setFPMode (intptr_t newValue) noexcept
{
   #if SOUL_ARM64
    asm volatile("msr fpcr, %0" : : "ri" (newValue));
   #elif SOUL_ARM32
    asm volatile("vmsr fpscr, %0" : : "ri" (newValue));
   #elif SOUL_INTEL
    auto v = static_cast<uint32_t> (newValue);
    _mm_setcsr (v);
   #else
    (void) newValue;
   #endif
}

}
