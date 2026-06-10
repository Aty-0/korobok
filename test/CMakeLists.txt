cmake_minimum_required(VERSION 3.20)

project(krb_test LANGUAGES CXX)


set(KRB_TEST_SOURCE_FILENAME "krb_test_source.krb")
set(KRB_BUILD_PATH "$<TARGET_FILE_DIR:krb_test>")

add_executable(krb_test src/main.cpp)

target_link_libraries(krb_test PRIVATE krb::krb)
target_compile_features(krb_test PRIVATE cxx_std_20)
target_compile_definitions(krb_test PRIVATE 
    KRB_ENABLE_LOGS
    KRB_TEST_SOURCE_FILENAME="${KRB_TEST_SOURCE_FILENAME}"
)

add_test(NAME krb_test COMMAND krb_test)

add_custom_target(GenerateTestSourceFile ALL 
        COMMAND ${CMAKE_COMMAND}        
        -DKRB_TEST_SOURCE_FILENAME=${KRB_TEST_SOURCE_FILENAME}
        -DKRB_BUILD_PATH=${KRB_BUILD_PATH}
        -DKRB_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CreateSource.cmake
        COMMENT "generate test source"
)

add_dependencies(krb_test GenerateTestSourceFile)