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

#define SOUL_ERRORS_SYNTAX(X) \
    X(staticAssertionFailure,               "Static assertion failure") \
    X(staticAssertionFailureWithMessage,    "$0$") \
    X(staticAssertionNotAllowed,            "static_assert must be used inside a function") \
    X(identifierTooLong,                    "Identifier too long") \
    X(invalidUTF8,                          "Invalid UTF8 data") \
    X(noLeadingUnderscoreAllowed,           "Identifiers beginning with an underscore are reserved for system use") \
    X(illegalCharacter,                     "Unexpected character $Q0$ in source") \
    X(unterminatedComment,                  "Unterminated '/*' comment") \
    X(integerLiteralTooLarge,               "Integer literal is too large to be represented") \
    X(integerLiteralNeedsSuffix,            "This value is too large to fit into an int32, did you mean to add an 'i64' suffix?") \
    X(unrecognisedLiteralSuffix,            "Unrecognised suffix on literal") \
    X(noOctalLiterals,                      "Octal literals are not supported") \
    X(decimalDigitInOctal,                  "Decimal digit in octal constant") \
    X(errorInNumericLiteral,                "Syntax error in numeric constant") \
    X(errorInEscapeCode,                    "Syntax error in unicode escape sequence") \
    X(endOfInputInStringConstant,           "Unexpected end-of-input in string constant") \
    X(expected,                             "Expected $0$") \
    X(foundWhenExpecting,                   "Found $0$ when expecting $1$") \
    X(expectedExpressionOfType,             "Expected an expression of type $0$") \
    X(expectedType,                         "Expected a type") \
    X(expectedConstant,                     "Expected a constant value") \
    X(expectedValue,                        "Expected a value") \
    X(expectedValueOrEndpoint,              "Expected a value or endpoint") \
    X(expectedProcessorName,                "Expected a processor name") \
    X(expectedNamespaceName,                "Expected a namespace name") \
    X(expectedProcessorOrEndpoint,          "Expected a processor name or endpoint") \
    X(expectedStatement,                    "Expected a statement") \
    X(expectedInteger,                      "Expected an integer") \
    X(expectedArrayOrVector,                "Expected an array or vector type") \
    X(expectedFunctionOrVariable,           "Expected a function or variable declaration") \
    X(expectedGenericWildcardName,          "Expected a generic function wildcard name") \
    X(expectedTopLevelDecl,                 "Expected a graph, processor or namespace declaration") \
    X(expectedVariableDecl,                 "Expected a variable declaration") \
    X(expectedInterpolationType,            "Expected an interpolation type") \
    X(expectedStreamType,                   "Expected a stream type specifier") \
    X(expectedVersionNumber,                "Expected a valid version number after the #SOUL directive") \
    X(expectedModule,                       "Expected a module identifier") \
    X(importsMustBeAtStart,                 "Import statements can only be declared at the start of a namespace") \
    X(namespaceMustBeInsideNamespace,       "A namespace can only be defined inside a namespace") \
    X(processorMustBeInsideNamespace,       "A processor can only be defined inside a namespace") \
    X(graphMustBeInsideNamespace,           "A graph can only be defined inside a namespace") \
    X(graphCannotContainFunctions,          "Functions can only be declared inside a processor or namespace") \
    X(namespaceCannotContainEndpoints,      "A namespace cannot contain endpoint declarations") \
    X(structDeclNotAllowed,                 "A struct can only be declared inside a processor or namespace") \
    X(usingDeclNotAllowed,                  "A using declaration can only be within a processor or namespace") \
    X(noFunctionInThisScope,                "Cannot declare a function in this scope") \
    X(noVariableInThisScope,                "Cannot declare a variable in this scope") \
    X(noEventFunctionsAllowed,              "Event handlers can only be declared inside a processor") \
    X(endpointDeclsMustBeFirst,             "Endpoint declarations must all appear at the start of the processor") \
    X(cannotResolveChildEndpoint,           "Cannot resolve child endpoint reference") \
    X(graphCannotHaveSpecialisations,       "Graphs cannot have type specialisations") \
    X(processorSpecialisationNotAllowed,    "Processor specialisations may only be used in graphs") \
    X(namespaceSpecialisationNotAllowed,    "Namespace specialisations may only be used in namespaces") \
    X(wrongAPIVersion,                      "Cannot parse code that was generated by a later version of the API") \
    X(semicolonAfterBrace,                  "A brace-enclosed declaration should not be followed by a semicolon") \
    X(nameInUse,                            "The name $Q0$ is already in use") \
    X(invalidEndpointName,                  "The name $Q0$ is not a valid endpoint name") \
    X(alreadyProcessorWithName,             "There is already a processor called $Q0$ visible in this scope") \
    X(invalidName,                          "Invalid name $Q0$") \
    X(invalidVariableName,                  "Invalid variable name $Q0$") \
    X(invalidBlockName,                     "Invalid block name $Q0$") \
    X(invalidIdentifierName,                "Invalid identifier name $Q0$") \
    X(nameTooLong,                          "The name $Q0$ exceeded the maximum length") \
    X(notYetImplemented,                    "Language feature not yet implemented: $0$!") \
    X(cannotFind,                           "Cannot find $Q0$") \
    X(unresolvedSymbol,                     "Cannot find symbol $Q0$") \
    X(unresolvedType,                       "Cannot find type $Q0$") \
    X(cannotFindSource,                     "Cannot find source $Q0$") \
    X(cannotFindDestination,                "Cannot find destination $Q0$") \
    X(cannotFindInput,                      "Cannot find input $Q0$") \
    X(cannotFindOutput,                     "Cannot find output $Q0$") \
    X(cannotFindEndpoint,                   "Cannot find endpoint $Q0$") \
    X(cannotConnectFromAnInput,             "The endpoint $Q0$ is an input, so cannot be connected to $Q1$") \
    X(cannotConnectToAnOutput,              "The endpoint $Q1$ is an output, so cannot take an input from $Q0$") \
    X(processorHasNoSuitableInputs,         "This processor has no suitable input endpoints") \
    X(processorHasNoSuitableOutputs,        "This processor has no suitable output endpoints") \
    X(mustBeOnlyOneEndpoint,                "A processor can only be placed inside a chain if it has exactly one input and one output") \
    X(cannotNameEndpointInChain,            "A processor that is chained between two others cannot specify an endpoint name") \
    X(cannotChainConnectionWithMultiple,    "Cannot create a chained sequence of connections when multiple endpoints are specified") \
    X(invalidEndpointSpecifier,             "Invalid endpoint specifier") \
    X(sourceEndpointIndexOutOfRange,        "Source index out of range") \
    X(destinationEndpointIndexOutOfRange,   "Destination index out of range") \
    X(ambiguousSymbol,                      "Multiple matches found when looking for $Q0$") \
    X(unknownMemberInStruct,                "No member called $Q0$ found in struct $Q1$") \
    X(unknownMemberInComplex,               "$1$ has no member called $Q0$") \
    X(notAProcessorOrGraph,                 "$Q0$ is not a processor or graph") \
    X(noSuchOperationOnEndpoint,            "No such operation is supported on an endpoint") \
    X(noSuchOperationOnProcessor,           "No such operation is supported on a processor") \
    X(expectedStructForDotOperator,         "Expected a struct type to the left of the dot operator") \
    X(invalidDotArguments,                  "Invalid arguments for the dot operator") \
    X(feedbackInGraph,                      "Feedback cycle in graph: $0$") \
    X(cannotFindProcessor,                  "Cannot find processor $Q0$") \
    X(cannotFindMainProcessorWithName,      "Cannot find a main processor matching the name $Q0$") \
    X(cannotFindMainProcessor,              "Cannot find a main processor or graph to use") \
    X(multipleProcessorsMarkedAsMain,       "Multiple processors were marked as 'main'") \
    X(onlyOneHeartFileAllowed,              "When compiling HEART code, only a single module must be provided") \
    X(duplicateTypesInList,                 "Duplicate types found in type list: $0$ and $1$") \
    X(unresolvedAnnotation,                 "Cannot resolve constant value in annotation") \
    X(illegalTypeForEndpoint,               "Only primitives or vectors supported by this endpoint type") \
    X(illegalTypeForEndpointArray,          "Endpoint arrays do not support array data types") \
    X(voidCannotBeUsedForEndpoint,          "void is not a valid endpoint type") \
    X(noMultipleTypesOnEndpoint,            "Multiple data types not supported by this endpoint type") \
    X(endpointHasMultipleTypes,             "This endpoint has more than one type") \
    X(incompatibleRatesOnEndpoints,         "Endpoints have incompatible sample rates: $0$ and $1$") \
    X(noSampleRateForEndpoint,              "No endpoint specifies a valid sample rate") \
    X(onlyOneTypeInTopLevelInputs,          "Top level input endpoints can only declare one type") \
    X(wrongTypeForEndpoint,                 "This type is not supported by the endpoint") \
    X(cannotWriteTypeToEndpoint,            "Cannot write type $0$ to endpoint which takes $1$") \
    X(incompatibleEndpointType,             "Incompatible endpoint type") \
    X(endpointIndexOutOfRange,              "Endpoint index out of range") \
    X(endpointIndexInvalid,                 "Endpoint index is not valid") \
    X(recursiveTypes,                       "Recursively nested types within $Q0$") \
    X(typeContainsItself,                   "The type $Q0$ cannot contain itself recursively") \
    X(typesReferToEachOther,                "The types $Q0$ and $Q1$ refer to each other recursively") \
    X(initialiserRefersToTarget,            "The variable $Q0$ cannot recursively refer to itself in its initial value") \
    X(cannotTakeSizeOfType,                 "Cannot take the size of this type") \
    X(tooManyConsts,                        "The 'const' keyword cannot be applied to a type that is already const") \
    X(badTypeForElementType,                "'elementType' can only be applied to an array or vector type") \
    X(badTypeForPrimitiveType,              "'primitiveType' can only be applied to a vector or primitive type") \
    X(cannotReadFromOutput,                 "Cannot read from an output") \
    X(cannotReadFromEventInput,             "Event inputs must be handled in event callback functions, they cannot be read as expressions") \
    X(cannotUseProcessorAsValue,            "Cannot use a processor name as a value") \
    X(cannotUseProcessorAsType,             "Cannot use a processor name as a type") \
    X(cannotCastBetween,                    "Cannot convert type $Q0$ to $Q1$") \
    X(cannotCastValue,                      "Cannot convert $0$ ($Q1$) to $Q2$") \
    X(cannotCastListToType,                 "Cannot convert comma-separated list to type $Q0$") \
    X(cannotImplicitlyCastValue,            "Cannot implicitly convert $0$ ($Q1$) to $Q2$") \
    X(cannotImplicitlyCastType,             "Cannot implicitly convert $Q0$ to $Q1$") \
    X(ambiguousCastBetween,                 "Ambiguous cast from type $Q0$ to $Q1$") \
    X(wrongNumArgsForAggregate,             "Wrong number of values to create a type $Q0$") \
    X(tooManyElements,                      "Too many elements") \
    X(delayLineMustBeConstant,              "A delay line length must be a constant") \
    X(delayLineMustHaveIntLength,           "A delay line length must be an integer") \
    X(delayLineTooShort,                    "A delay line length must be greater than zero") \
    X(delayLineTooLong,                     "Illegal delay line length") \
    X(endpointIndexMustBeConstant,          "Endpoint index must be a constant") \
    X(duplicateFunction,                    "A function with matching parameters has already been defined") \
    X(duplicateProcessor,                   "A processor with the name $Q0$ has already been declared") \
    X(duplicateModule,                      "A module with the name $Q0$ has already been declared") \
    X(processorNeedsAnOutput,               "A processor must declare at least one output") \
    X(functionMustBeVoid,                   "The $0$() function must return 'void'") \
    X(functionHasParams,                    "The $0$() function must not have any parameters") \
    X(processorNeedsRunFunction,            "A processor must contain a run() function") \
    X(multipleRunFunctions,                 "A processor cannot contain more than one run() function") \
    X(cannotCallFunction,                   "The $0$() function cannot be called from user code") \
    X(runFunctionMustCallAdvance,           "The run() function must call advance()") \
    X(advanceIsNotAMethod,                  "The advance() function cannot be used as a method call") \
    X(advanceHasNoArgs,                     "The advance() function does not take any arguments") \
    X(advanceCannotBeCalledHere,            "The advance() function cannot be called inside this function") \
    X(streamsCanOnlyBeUsedInRun,            "Streams can only be read or written inside the run() function") \
    X(streamsCannotBeUsedDuringInit,        "Streams are not available during init()") \
    X(streamsCannotBeUsedInEventCallbacks,  "Streams cannot be used in event callback functions") \
    X(noSuchInputEvent,                     "The event $Q0$ does not match an event input") \
    X(variableCannotBeVoid,                 "A variable type cannot be 'void'") \
    X(parameterCannotBeVoid,                "Function parameters cannot be void") \
    X(typeCannotBeReference,                "This type cannot be a reference") \
    X(memberCannotBeConst,                  "Struct members cannot be declared const") \
    X(memberCannotBeReference,              "Struct members cannot be references") \
    X(processorParamsCannotBeReference,     "Processor parameter types cannot be references") \
    X(externalNeedsInitialiser,             "External variables cannot be given an initialiser value") \
    X(externalNotAllowedInFunction,         "External constants cannot be declared inside a function") \
    X(latencyMustBeConstInteger,            "The processor.latency value must be declared as an integer constant") \
    X(latencyOutOfRange,                    "This latency value is out of range") \
    X(latencyOnlyForProcessor,              "The processor.latency value can only be declared in a processor") \
    X(latencyAlreadyDeclared,               "The processor.latency value must not be set more than once") \
    X(cannotReferenceOtherProcessorVar,     "Cannot reference a mutable variable belonging to another processor") \
    X(externalOnlyAllowedOnStateVars,       "The 'external' flag can only be applied to state variables") \
    X(wrongTypeForUnary,                    "Illegal type for unary operator") \
    X(identifierMustBeUnqualified,          "This identifier cannot have a namespace qualifier") \
    X(nonConstInNamespace,                  "Only constant variables can be declared inside a namespace") \
    X(nonConstInGraph,                      "Only constant variables can be declared inside a graph") \
    X(comparisonAlwaysTrue,                 "Comparison with bounded integer type is always true") \
    X(comparisonAlwaysFalse,                "Comparison with bounded integer type is always false") \
    X(illegalArraySize,                     "Illegal array size") \
    X(targetIsNotAnArray,                   "The target for this expression is not an array") \
    X(illegalSliceSize,                     "Invalid array slice range") \
    X(cannotCreateSliceFromValue,           "Cannot create a dynamic array slice from this value") \
    X(nonIntegerArraySize,                  "Array or vector size must be an integer") \
    X(nonIntegerArrayIndex,                 "An array index must be an integer type") \
    X(nonConstArraySize,                    "Expected a constant value for the array size") \
    X(negativeLoopCount,                    "Number of iterations must be a positive number") \
    X(rangeBasedForMustBeWrapType,          "A range-based-for loop must declare a variable with a 'wrap' type") \
    X(preIncDecCollision,                   "Variables which have the ++ or -- operator applied can not be used twice within the same statement") \
    X(functionCannotBeExternal,             "A function declaration cannot be marked 'external'") \
    X(noConstOnExternals,                   "External declarations do not require the 'const' keyword") \
    X(usingCannotBeReference,               "Using declarations cannot be references") \
    X(expectedUnqualifiedName,              "This name cannot have a namespace qualifier") \
    X(qualifierOnGeneric,                   "Generic function types must be a non-qualified identifier") \
    X(tooManyParameters,                    "Too many function parameters") \
    X(tooManyInitialisers,                  "Initialiser list exceeds max length limit") \
    X(cannotPassConstAsNonConstRef,         "Cannot pass a const value as a non-const reference") \
    X(assignmentInsideExpression,           "Assignment is not allowed inside an expression") \
    X(propertiesOutsideProcessor,           "Processor properties are only valid inside a processor declaration") \
    X(cannotAssignToProcessorProperties,    "Processor properties are constants, and cannot be modified") \
    X(typeReferenceNotAllowed,              "Type references are not allowed in this context") \
    X(processorReferenceNotAllowed,         "Processor references are not allowed in this context") \
    X(cannotResolveSpecialisationValue,     "Cannot resolve value") \
    X(eventTypeCannotBeReference,           "Event types cannot be references") \
    X(eventFunctionInvalidType,             "Event $Q0$ does not support type $Q1$") \
    X(eventFunctionInvalidArguments,        "Event function arguments invalid") \
    X(eventParamsCannotBeNonConstReference, "Event parameters cannot be non-const references") \
    X(wrongNumberOfComplexInitialisers,     "Too many initialisers for complex number") \
    X(wrongTypeForInitialiseList,           "You can only create a multi-value initialiser list for an array, vector or struct") \
    X(wrongTypeForArrayElement,             "Cannot create an array with this element type") \
    X(wrongTypeForVectorElement,            "Cannot create a vector with elements that are not primitive types") \
    X(arraySizeMustBeConstant,              "An array size must be a constant") \
    X(illegalVectorSize,                    "Illegal vector size") \
    X(illegalSize,                          "Illegal size") \
    X(wrapOrClampSizeMustBeConstant,        "The size of a 'wrap' or 'clamp' type must be a constant") \
    X(propertyMustBeConstant,               "Property values must be compile-time constants") \
    X(illegalPropertyType,                  "Unsupported property value data type") \
    X(arraySuffixOnProcessor,               "Cannot use an array suffix on a processor name in this context") \
    X(cannotResolveVectorSize,              "Cannot resolve vector size expression in this context") \
    X(cannotResolveBracketedExp,            "Cannot resolve bracketed expression in this context") \
    X(cannotResolveSourceType,              "Cannot resolve source type") \
    X(illegalTypesForBinaryOperator,        "Illegal types for binary operator $Q0$ ($Q1$ and $Q2$)") \
    X(inPlaceOperatorMustBeStatement,       "The in-place operator $Q0$ must be used as a statement, not an expression") \
    X(cannotOperateOnArrays,                "The $Q0$ operator can be applied to vector types, but not arrays") \
    X(eventFunctionIndexInvalid,            "Event Handlers for event arrays need a first argument index integer type") \
    X(noMatchForFunctionCall,               "No suitable override found for function call: $0$") \
    X(ambiguousFunctionCall,                "Ambiguous function call: $0$") \
    X(noFunctionWithNumberOfArgs,           "Can't find a function $Q0$ with $1$ argument(s)") \
    X(cannotUseProcessorAsFunction,         "Cannot use a processor name as a function call") \
    X(cannotUseInputAsFunction,             "Cannot use an input as a function call") \
    X(cannotUseOutputAsFunction,            "Cannot use an output as a function call") \
    X(unknownFunction,                      "Unknown function: $Q0$") \
    X(unknownFunctionWithSuggestion,        "Unknown function: $Q0$ (did you mean $Q1$?)") \
    X(expected1or2Args,                     "Expected 1 or 2 arguments") \
    X(wrongNumArgsForProcessor,             "Wrong number of arguments to instantiate processor $Q0$") \
    X(wrongNumArgsForNamespace,             "Wrong number of arguments to instantiate namespace $Q0$") \
    X(cannotUseProcessorInLet,              "The processor $Q0$ cannot be used in a 'let' statement if it is also used directly in a connection") \
    X(cannotReuseImplicitProcessorInstance, "An implicitly-created processor cannot be used more than once: create a named instance instead") \
    X(cannotResolveFunctionOrCast,          "Could not resolve function or cast") \
    X(voidFunctionCannotReturnValue,        "A void function cannot return a value") \
    X(functionReturnTypeCannotBeConst,      "Function return type cannot be const") \
    X(functionContainsAnInfiniteLoop,       "The function $Q0$ contains at least one infinite loop") \
    X(notAllControlPathsReturnAValue,       "Not all control paths in the function $Q0$ return a value") \
    X(functionCallsItselfRecursively,       "The function $0$ calls itself recursively") \
    X(functionsCallEachOtherRecursively,    "The functions $0$ and $1$ call each other recursively") \
    X(recursiveFunctionCallSequence,        "Recursive call sequence via functions: $0$") \
    X(expressionHasNoEffect,                "This constant expression will have no effect") \
    X(unusedExpression,                     "Result of this expression is unused") \
    X(expectedStringLiteralAsArg2,          "Expected a string literal error message as the second argument") \
    X(atMethodTakes1Arg,                    "The 'at' method expects one argument") \
    X(cannotResolveSourceOfAtMethod,        "Cannot resolve the source of the 'at' method") \
    X(wrongTypeForAtMethod,                 "The 'at' method can only be applied to a vector or array") \
    X(ternaryCannotBeVoid,                  "The ternary operator must return non-void values") \
    X(ternaryTypesMustMatch,                "Ternary operator branches have different types ($Q0$ and $Q1$)") \
    X(ternaryCannotBeStatement,             "A ternary operator cannot be used as a statement") \
    X(moduloZero,                           "Modulo zero is undefined behaviour") \
    X(divideByZero,                         "Divide-by zero is undefined behaviour") \
    X(operatorNeedsAssignableTarget,        "The $Q0$ operator must be given an assignable variable") \
    X(expressionNotAssignable,              "This expression cannot be used as the target for an assignment") \
    X(illegalTypeForOperator,               "Illegal type for the $Q0$ operator") \
    X(cannotUseBracketOnEndpoint,           "Cannot use the bracket operator on this endpoint") \
    X(expectedArrayOrVectorForBracketOp,    "Expected a vector or array to the left of the bracket operator") \
    X(cannotUseBracketsOnNonArrayEndpoint,  "Cannot use operator[] to reference endpoints which are not arrays") \
    X(indexOutOfRange,                      "Index is out of range") \
    X(targetMustBeOutput,                   "The target for the write operator must be an output") \
    X(ratioMustBeConstant,                  "Expected a constant value for the ratio") \
    X(ratioMustBeInteger,                   "Clock ratio must be an integer constant") \
    X(ratioOutOfRange,                      "Clock ratio out of range") \
    X(ratioMustBePowerOf2,                  "Clock ratio must be a power of 2") \
    X(unsupportedSincClockRatio,            "Clock ratio not supported by sinc interpolator") \
    X(codeCacheConsistencyFail,             "Code cache consistency failure") \
    X(cannotAssignToDynamicElement,         "Cannot assign to an element of a dynamic array") \
    X(unresolvedExternal,                   "Failed to resolve external variable $Q0$") \
    X(cannotConvertExternalType,            "Cannot convert value for external from $Q0$ to $Q1$") \
    X(incompatibleInputInterpolationTypes,  "Incompatible interpolation types for module inputs $Q0$") \
    X(incompatibleOutputInterpolationTypes, "Incompatible interpolation types for module outputs $Q0$") \
    X(cannotConnectSourceAndSink,           "Cannot connect an source of type $0$ to a destination of type $1$") \
    X(cannotConnect,                        "Cannot connect $0$ ($1$) to $2$ ($3$)") \
    X(incompatibleTargetType,               "Incompatible target type") \
    X(unsupportedType,                      "Unsupported type") \
    X(emptyProgram,                         "Program is empty") \
    X(processorPropertyUsedOutsideDecl,     "Processor properties are only valid inside a processor declaration") \
    X(unknownProperty,                      "Unknown processor property name") \
    X(cannotNegateConstant,                 "Cannot negate this type of constant") \
    X(useOfUninitialisedVariable,           "Use of uninitialised variable $Q0$ in function $1$") \
    X(functionHasNoImplementation,          "This function has no implementation") \
    X(functionBlockCantBeParameterised,     "Function block $0$ cannot be parameterised") \
    X(branchInvalidParameters,              "Block $0$ terminator has invalid block paramteters") \
    X(blockParametersInvalid,               "Block $0$ parameters invalid") \
    X(missingAnnotationValue,               "Missing annotation value") \
    X(emptyFunction,                        "Function $0$ is empty") \
    X(tooManyNamespaceInstances,            "Exceeded the maximum number of specialised namespace instances ($0$) - possible namespace recursion") \
    X(circularNamespaceAlias,               "Circular reference in namespace alias definition $Q0$") \

#define SOUL_ERRORS_LIMITS(X) \
    X(programStateTooLarge,                 "Program state requires $0$, maximum allowed is $1$") \
    X(maximumStackSizeExceeded,             "Stack size limit exceeded - program requires $0$, maximum allowed is $1$") \
    X(unsupportedBitDepth,                  "Unsupported bit-depth") \
    X(unsupportedBlockSize,                 "Unsupported block size") \
    X(unsupportedSampleRate,                "Unsupported sample rate") \
    X(unsupportedOptimisationLevel,         "Unsupported optimisation level") \
    X(unsupportedNumChannels,               "Unsupported number of channels") \

#define SOUL_ERRORS_RUNTIME(X) \
    X(customRuntimeError,                   "$0$") \
    X(failedToLoadProgram,                  "Failed to load program") \
    X(cannotOverwriteFile,                  "Cannot overwrite existing file $Q0$") \
    X(cannotCreateOutputFile,               "Cannot create output file $Q0$") \
    X(cannotCreateFolder,                   "Cannot create folder $Q0$") \
    X(cannotReadFile,                       "Failed to read from file $Q0$") \
    X(cannotLoadLibrary,                    "Cannot load library $Q0$") \
    X(processTookTooLong,                   "Processing took too long") \


//==============================================================================
#define SOUL_WARNINGS_PERFORMANCE(X) \
    X(indexHasRuntimeOverhead,              "Performance warning: the type of this array index could not be proven to be safe, so a runtime check was added") \

#define SOUL_WARNINGS_SYNTAX(X) \
    X(localVariableShadow,                  "The variable $Q0$ shadows another local variable with the same name") \

//==============================================================================
struct CompileMessageHelpers
{
private:
    static constexpr int numberOfMatchesOfArg (const char* text, int index)
    {
        int matches = 0;

        for (; *text != 0; ++text)
        {
            if (*text == '$')
            {
                ++text;

                if (*text == 'Q')
                    ++text;

                if (*text < '0' || *text > '9')
                    return -1;

                if (index == (*text - '0'))
                    ++matches;

                ++text;

                if (*text != '$')
                    return -1;
            }
        }

        return matches;
    }

    static std::string replaceArgument (std::string text, size_t index, const std::string& value)
    {
        auto i = std::to_string (index);
        text = choc::text::replace (text, "$Q" + i + "$", choc::text::addSingleQuotes (value));
        return choc::text::replace (text, "$" + i + "$", value);
    }

public:
    template <typename... Args>
    static CompileMessage createMessage (CompileMessage::Category category, CodeLocation location,
                                         CompileMessage::Type type, const char* text, Args&&... args)
    {
        std::string result (text);
        std::vector<std::string> stringArgs { convertToString (args)... };

        for (size_t i = 0; i < stringArgs.size(); ++i)
            result = replaceArgument (result, i, stringArgs[i]);

        return CompileMessage { choc::text::trim (result), location, type, category };
    }

    template <typename... Args>
    static CompileMessage createMessage (CompileMessage::Category category, CompileMessage::Type type,
                                         const char* text, Args&&... args)
    {
        return createMessage (category, CodeLocation(), type, text, std::forward<Args> (args)...);
    }

    static constexpr int countArgs (const char* text)
    {
        for (int num = 0;; ++num)
        {
            auto numMatches = numberOfMatchesOfArg (text, num);

            if (numMatches < 0)
                return -1;

            if (numMatches == 0)
                return num;
        }
    }
};

//==============================================================================
struct Errors
{
    #define SOUL_DECLARE_ERROR_HANDLER(name, text, category) \
        template <typename... Args> static CompileMessage name (Args&&... args) \
        { static_assert (CompileMessageHelpers::countArgs (text) == sizeof... (args), "mismatched number of args for error"); \
          return CompileMessageHelpers::createMessage (category, CompileMessage::Type::error, text, std::forward<Args> (args)...); }

    #define SOUL_DECLARE_ERROR_HANDLER_SYNTAX(name, text)  SOUL_DECLARE_ERROR_HANDLER(name, text, CompileMessage::Category::syntax)
    #define SOUL_DECLARE_ERROR_HANDLER_LIMITS(name, text)  SOUL_DECLARE_ERROR_HANDLER(name, text, CompileMessage::Category::limitExceeded)
    #define SOUL_DECLARE_ERROR_HANDLER_RUNTIME(name, text) SOUL_DECLARE_ERROR_HANDLER(name, text, CompileMessage::Category::runtimeProblem)

    SOUL_ERRORS_SYNTAX   (SOUL_DECLARE_ERROR_HANDLER_SYNTAX)
    SOUL_ERRORS_LIMITS   (SOUL_DECLARE_ERROR_HANDLER_LIMITS)
    SOUL_ERRORS_RUNTIME  (SOUL_DECLARE_ERROR_HANDLER_RUNTIME)

    #undef SOUL_DECLARE_ERROR_HANDLER_SYNTAX
    #undef SOUL_DECLARE_ERROR_HANDLER_LIMITS
    #undef SOUL_DECLARE_ERROR_HANDLER_RUNTIME
    #undef SOUL_DECLARE_ERROR_HANDLER
};

//==============================================================================
struct Warnings
{
    #define SOUL_DECLARE_WARNING_HANDLER(name, text, category) \
        template <typename... Args> static CompileMessage name (Args&&... args) \
        { static_assert (CompileMessageHelpers::countArgs (text) == sizeof... (args), "mismatched number of args for warning"); \
          return CompileMessageHelpers::createMessage (category, CompileMessage::Type::warning, text, std::forward<Args> (args)...); }

    #define SOUL_DECLARE_WARNING_HANDLER_PERFORMANCE(name, text)    SOUL_DECLARE_WARNING_HANDLER(name, text, CompileMessage::Category::performanceProblem)
    #define SOUL_DECLARE_WARNING_HANDLER_SYNTAX(name, text)         SOUL_DECLARE_WARNING_HANDLER(name, text, CompileMessage::Category::syntax)

    SOUL_WARNINGS_PERFORMANCE(SOUL_DECLARE_WARNING_HANDLER_PERFORMANCE)
    SOUL_WARNINGS_SYNTAX(SOUL_DECLARE_WARNING_HANDLER_SYNTAX)

    #undef SOUL_DECLARE_WARNING_HANDLER_PERFORMANCE
    #undef SOUL_DECLARE_WARNING_HANDLER
};

//==============================================================================
struct DuplicateNameChecker
{
    std::vector<std::string> names;

    template <typename Thrower>
    void check (const std::string& nameToCheck, Thrower&& errorLocation)
    {
        checkWithoutAdding (nameToCheck, errorLocation);
        names.push_back (nameToCheck);
    }

    template <typename Thrower>
    void check (Identifier nameToCheck, Thrower&& errorLocation)
    {
        check (nameToCheck.toString(), errorLocation);
    }

    template <typename Thrower>
    void checkWithoutAdding (const std::string& nameToCheck, Thrower&& errorLocation)
    {
        for (auto& n : names)
            if (n == nameToCheck)
                errorLocation.throwError (Errors::nameInUse (nameToCheck));
    }

    template <typename Thrower>
    void checkWithoutAdding (Identifier nameToCheck, Thrower&& errorLocation)
    {
        checkWithoutAdding (nameToCheck.toString(), errorLocation);
    }
};

} // namespace soul
