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
    std::string minInt32Fn  = "_minInt32";
    std::string wrapInt32Fn = "_wrapInt32";

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
        auto& a  = builder.addParameter ("a", PrimitiveType::int32);
        auto& b  = builder.addParameter ("b", PrimitiveType::int32);

        auto& lessThan = builder.createBlock ("@lessThan");
        auto& moreThan = builder.createBlock ("@moreThan");

        builder.addBranchIf (builder.createComparisonOp (a, b, BinaryOp::Op::lessThan), lessThan, moreThan, lessThan);
        builder.addReturn (a);
        builder.beginBlock (moreThan);
        builder.addReturn (b);
    });

    auto& call = module.allocate<heart::PureFunctionCall> (a.location, function);
    call.arguments.push_back (a);
    call.arguments.push_back (b);
    return call;
}


heart::PureFunctionCall& BlockBuilder::createWrapInt32 (heart::Expression& n, heart::Expression& range)
{
    return createWrapInt32 (module, n, range);
}

    
heart::PureFunctionCall& BlockBuilder::createWrapInt32 (Module& module, heart::Expression& n, heart::Expression& range)
{
    SOUL_ASSERT (n.getType().isInteger32() && n.getType().isPrimitiveInteger() && range.getType().isInteger32());

    auto& internalModule = getInternalModule (module.program);

    auto& function = soul::FunctionBuilder::getOrCreateFunction (internalModule, wrapInt32Fn, PrimitiveType::int32, [] (FunctionBuilder& builder)
    {
        auto& n      = builder.addParameter ("n", PrimitiveType::int32);
        auto& range  = builder.addParameter ("range", PrimitiveType::int32);
        auto& a      = builder.createRegisterVariable (PrimitiveType::int32);

        auto& equals = builder.createBlock ("@equals");
        auto& notEquals = builder.createBlock ("@notEquals");

        builder.addBranchIf (builder.createComparisonOp (n, builder.createZeroInitialiser(PrimitiveType::int32), BinaryOp::Op::equals), equals, notEquals, equals);
        builder.addReturn (builder.createZeroInitialiser(PrimitiveType::int32));
        builder.beginBlock (notEquals);

        builder.addAssignment(a, builder.createBinaryOp(n.location, n, range, BinaryOp::Op::modulo));

        auto& moduloNegative = builder.createBlock ("@moduloNegative");
        auto& moduloPositive = builder.createBlock ("@moduloPositive");

        builder.addBranchIf (builder.createComparisonOp (a, builder.createZeroInitialiser(PrimitiveType::int32), BinaryOp::Op::lessThan), moduloNegative, moduloPositive, moduloNegative);
        builder.addReturn (builder.createBinaryOp (a.location, a, range, BinaryOp::Op::add));
        builder.beginBlock (moduloPositive);
        builder.addReturn (a);
    });

    auto& call = module.allocate<heart::PureFunctionCall> (n.location, function);
    call.arguments.push_back (n);
    call.arguments.push_back (range);
    return call;
}

}
