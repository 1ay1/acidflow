# acidflow.clap — the engine as a CLAP instrument. The CLAP SDK is header-only
# and fetched at configure time.
include(FetchContent)
FetchContent_Declare(clap
    URL      https://github.com/free-audio/clap/archive/refs/tags/1.2.2.tar.gz
    URL_HASH SHA256=1472bfe3b9f4eac3d7aca57d4a528f0c39709a2af82900253ce6a5b86e0b60e1
    # Headers only: point at a dir with no CMakeLists so nothing is added.
    SOURCE_SUBDIR include)
FetchContent_MakeAvailable(clap)

add_library(acidflow_clap MODULE clap_plugin.cpp)
target_include_directories(acidflow_clap PRIVATE ${clap_SOURCE_DIR}/include)
target_link_libraries(acidflow_clap PRIVATE acid_engine acid_warnings)
set_target_properties(acidflow_clap PROPERTIES
    PREFIX ""
    OUTPUT_NAME "acidflow"
    SUFFIX ".clap"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)
if(APPLE)
    set_target_properties(acidflow_clap PROPERTIES BUNDLE TRUE BUNDLE_EXTENSION clap)
endif()

# A host-free check: load the module, instantiate, process a note.
add_executable(clap_test clap_test.cpp)
target_include_directories(clap_test PRIVATE ${clap_SOURCE_DIR}/include)
target_link_libraries(clap_test PRIVATE ${CMAKE_DL_LIBS})
add_dependencies(clap_test acidflow_clap)
if(ACIDFLOW_BUILD_TESTS)
    add_test(NAME clap COMMAND clap_test $<TARGET_FILE:acidflow_clap>)
endif()
