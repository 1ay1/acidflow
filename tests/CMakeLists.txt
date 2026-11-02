add_executable(dsp_test dsp_test.cpp)
target_link_libraries(dsp_test PRIVATE acid_engine acid_warnings)
add_test(NAME dsp COMMAND dsp_test)

add_executable(engine_test engine_test.cpp)
target_link_libraries(engine_test PRIVATE acid_engine acid_warnings)
add_test(NAME engine COMMAND engine_test)
