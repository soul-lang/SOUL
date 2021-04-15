cmake_minimum_required(VERSION 3.16)

project(choc_tests)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR
    CMAKE_CXX_COMPILER_ID MATCHES "GNU")

    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
    set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG} -O0 -DDEBUG")
    set(CMAKE_CXX_FLAGS         "${CMAKE_CXX_FLAGS} -std=c++17 -Werror -Wall -Wextra -Wshadow -Wno-missing-field-initializers -Wstrict-aliasing -Wuninitialized -Wunused-parameter -Wconversion -Wsign-compare -Wreorder -Wsign-conversion -Wno-ignored-qualifiers -Wunreachable-code")
    set(CMAKE_C_FLAGS           "${CMAKE_C_FLAGS} -Wno-unused-result")

    if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
        set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -latomic")
    endif ()

    # Enable additional Clang warnings
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wint-conversion -Woverloaded-virtual -Wshorten-64-to-32 -Wconditional-uninitialized -Wconstant-conversion -Wunused-private-field -Wbool-conversion -Wextra-semi -Wshadow-uncaptured-local")
    endif()

    # Disable warnings about ABI changes
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-psabi")
    endif()

    if (WARNINGS_AS_ERRORS)
        message ("Treating warnings as errors")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
    endif ()

    if (PROFILE)
        message ("Enabling profile output")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pg")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
    endif ()

elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")

    set(CompilerFlags
            CMAKE_CXX_FLAGS
            CMAKE_CXX_FLAGS_DEBUG
            CMAKE_CXX_FLAGS_RELEASE
            CMAKE_C_FLAGS
            CMAKE_C_FLAGS_DEBUG
            CMAKE_C_FLAGS_RELEASE
            )

    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /WX /wd4068 /wd4996 /std:c++17 -DWIN32 /bigobj -DNOMINMAX")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4996 /wd4267 /wd5281 /wd5293 /wd5313 /wd4305 /wd4018")

endif()

add_executable(choc_tests main.cpp)

install(TARGETS choc_tests)

enable_testing()
add_test(NAME choc_tests COMMAND choc_tests)
