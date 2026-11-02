add_executable(dsp_test dsp_test.cpp)
target_link_libraries(dsp_test PRIVATE acid_engine acid_warnings)
add_test(NAME dsp COMMAND dsp_test)

add_executable(engine_test engine_test.cpp)
target_link_libraries(engine_test PRIVATE acid_engine acid_warnings)
add_test(NAME engine COMMAND engine_test)

# UI tests run the real App on jaal::headless. Audio and MIDI are stubbed so
# no device is opened.
if(TARGET acid_app)
    add_executable(ui_test ui_test.cpp stubs.cpp)
    target_link_libraries(ui_test PRIVATE acid_app)
    add_test(NAME ui COMMAND ui_test)
endif()
