add_executable(dsp_test dsp_test.cpp)
target_link_libraries(dsp_test PRIVATE acid_engine acid_warnings)
add_test(NAME dsp COMMAND dsp_test)

add_executable(engine_test engine_test.cpp)
target_link_libraries(engine_test PRIVATE acid_engine acid_warnings)
# The real-time check asserts on speed, which flakes on loaded CI runners, so
# it runs as its own `perf`-labelled test (ctest -LE perf skips it).
add_test(NAME engine COMMAND engine_test --skip headroom)
add_test(NAME engine_perf COMMAND engine_test headroom)
set_tests_properties(engine_perf PROPERTIES LABELS perf)

# UI tests run the real App on jaal::headless. Audio and MIDI are stubbed so
# no device is opened.
if(TARGET acid_app)
    add_executable(ui_test ui_test.cpp stubs.cpp)
    target_link_libraries(ui_test PRIVATE acid_app)
    add_test(NAME ui COMMAND ui_test)
endif()

if(ACIDFLOW_LINK)
    add_executable(link_test link_test.cpp)
    target_include_directories(link_test SYSTEM PRIVATE
        ${link_SOURCE_DIR}/include ${asio_SOURCE_DIR}/asio/include)
    target_link_libraries(link_test PRIVATE acid_engine)
    get_target_property(_defs acid_engine COMPILE_DEFINITIONS)
    target_compile_definitions(link_test PRIVATE ${_defs})
    # Needs multicast on loopback; label it so sandboxed CI can skip it.
    add_test(NAME link COMMAND link_test)
    set_tests_properties(link PROPERTIES LABELS net)
endif()
