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

namespace
{
    constexpr auto minInt32Fn  = "_minInt32";
    constexpr auto wrapInt32Fn = "_wrapInt32";
    constexpr auto wrapInt64Fn = "_wrapInt64";

    static soul::Module& getInternalModule (soul::Program& p)
    {
        return p.getOrCreateNamespace ("_internal");
    }
}

namespace soul
{

heart::PureFunctionCall& BlockBuilder::createMinInt32 (heart::Expression& a, heart::Expression& b)
{
    SOUL_ASSERT (a.getType().isInteger32() && b.getType().isInteger32());

    auto& internalModule = getInternalModule (module.program);

    auto& function = soul::FunctionBuilder::getOrCreateFunction (internalModule, minInt32Fn, PrimitiveType::int32, [] (FunctionBuilder& builder)
    {
        auto& paramA = builder.addParameter ("a", PrimitiveType::int32);
        auto& paramB = builder.addParameter ("b", PrimitiveType::int32);

        auto& lessThan = builder.createBlock ("@lessThan");
        auto& moreThan = builder.createBlock ("@moreThan");

        builder.addBranchIf (builder.createComparisonOp (paramA, paramB, BinaryOp::Op::lessThan), lessThan, moreThan, lessThan);
        builder.addReturn (paramA);
        builder.beginBlock (moreThan);
        builder.addReturn (paramB);
    });

    auto& call = module.allocate<heart::PureFunctionCall> (a.location, function);
    call.arguments.push_back (a, b);
    return call;
}

heart::PureFunctionCall& BlockBuilder::createWrapInt32 (heart::Expression& n, heart::Expression& range)
{
    return createWrapInt32 (module, n, range);
}

heart::PureFunctionCall& BlockBuilder::createWrapInt32 (Module& module, heart::Expression& value, heart::Expression& rangeLimit)
{
    const auto& valueType = value.getType();
    SOUL_ASSERT ((valueType.isInteger32() || valueType.isInteger64()) && valueType.isPrimitive()
                  && rangeLimit.getType().isInteger32() && rangeLimit.getType().isPrimitive());

    auto& internalModule = getInternalModule (module.program);
    auto argType = valueType.isInteger32() ? PrimitiveType::int32 : PrimitiveType::int64;
    auto name = valueType.isInteger32() ? wrapInt32Fn : wrapInt64Fn;

    auto& function = soul::FunctionBuilder::getOrCreateFunction (internalModule, name, PrimitiveType::int32, [argType] (FunctionBuilder& builder)
    {
        auto& valueParam  = builder.addParameter ("n", argType);
        auto& rangeParam  = builder.addParameter ("range", PrimitiveType::int32);

        auto& equalsBlock    = builder.createBlock ("@equals");
        auto& notEqualsBlock = builder.createBlock ("@notEquals");

        builder.addBranchIf (builder.createComparisonOp (valueParam, builder.createZeroInitialiser (argType), BinaryOp::Op::equals),
                             equalsBlock, notEqualsBlock, equalsBlock);
        builder.addReturn (builder.createZeroInitialiser (PrimitiveType::int32));
        builder.beginBlock (notEqualsBlock);

        auto& rangeParamCast = builder.createCastIfNeeded (rangeParam, argType);
        auto& valueModRange = builder.createRegisterVariable (argType);
        builder.addAssignment (valueModRange, builder.createBinaryOp (CodeLocation(), valueParam, rangeParamCast, BinaryOp::Op::modulo));

        auto& moduloNegativeBlock = builder.createBlock ("@moduloNegative");
        auto& moduloPositiveBlock = builder.createBlock ("@moduloPositive");

        builder.addBranchIf (builder.createComparisonOp (valueModRange, builder.createZeroInitialiser (argType), BinaryOp::Op::lessThan),
                             moduloNegativeBlock, moduloPositiveBlock, moduloNegativeBlock);
        builder.addReturn (builder.createCastIfNeeded (builder.createBinaryOp (CodeLocation(), valueModRange, rangeParamCast, BinaryOp::Op::add),
                                                       PrimitiveType::int32));
        builder.beginBlock (moduloPositiveBlock);
        builder.addReturn (builder.createCastIfNeeded (valueModRange, PrimitiveType::int32));
    });

    auto& call = module.allocate<heart::PureFunctionCall> (value.location, function);
    call.arguments.push_back (value, rangeLimit);
    return call;
}

}
