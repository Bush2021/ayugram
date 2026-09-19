# This is the source code of AyuGram for Desktop.
#
# We do not and cannot prevent the use of our code,
# but be respectful and credit the original author.
#
# Copyright @Bush2021, 2026

add_library(td_ayu_secret_scheme OBJECT)
init_non_host_target(td_ayu_secret_scheme)
add_library(ayugram::td_ayu_secret_scheme ALIAS td_ayu_secret_scheme)

function(generate_secret_scheme target_name script scheme_file)
    find_package(Python3 REQUIRED)

    set(gen_dst ${CMAKE_CURRENT_BINARY_DIR}/gen)
    file(MAKE_DIRECTORY ${gen_dst})

    set(gen_timestamp ${gen_dst}/secret_scheme.timestamp)
    set(gen_files
        ${gen_dst}/secret_scheme.cpp
        ${gen_dst}/secret_scheme.h
    )

    add_custom_command(
    OUTPUT
        ${gen_timestamp}
    BYPRODUCTS
        ${gen_files}
    COMMAND
        ${Python3_EXECUTABLE}
        ${script}
        -o${gen_dst}/secret_scheme
        ${scheme_file}
    COMMENT "Generating secret scheme (${target_name})"
    DEPENDS
        ${script}
        ${submodules_loc}/lib_tl/tl/generate_tl.py
        ${scheme_file}
    )
    generate_target(${target_name} secret_scheme ${gen_timestamp} "${gen_files}" ${gen_dst})
endfunction()

generate_secret_scheme(td_ayu_secret_scheme
    ${src_loc}/ayu/secret/scheme/codegen_secret_scheme.py
    ${src_loc}/ayu/secret/scheme/secret_api.tl)

nice_target_sources(td_ayu_secret_scheme ${src_loc}/ayu/secret/scheme
PRIVATE
    secret_api.tl
)

target_include_directories(td_ayu_secret_scheme
PUBLIC
    ${src_loc}
)

target_link_libraries(td_ayu_secret_scheme
PUBLIC
    desktop-app::lib_base
    desktop-app::lib_tl
    tdesktop::td_scheme
)

add_executable(test_ayu_secret EXCLUDE_FROM_ALL)
init_target(test_ayu_secret "(tests)")

target_precompile_headers(test_ayu_secret
PRIVATE
    ${src_loc}/ayu/secret/tests/secret_test_pch.h
)

nice_target_sources(test_ayu_secret ${src_loc}
PRIVATE
    ayu/secret/secret_crypto.cpp
    ayu/secret/secret_crypto.h
    ayu/secret/secret_chat_state.cpp
    ayu/secret/secret_chat_state.h
    ayu/secret/tests/secret_crypto_test.cpp
    mtproto/details/mtproto_dump_to_text.cpp
    mtproto/details/mtproto_dump_to_text.h
    mtproto/mtproto_auth_key.cpp
    mtproto/mtproto_auth_key.h
)

target_include_directories(test_ayu_secret PRIVATE ${src_loc})

target_link_libraries(test_ayu_secret
PRIVATE
    desktop-app::lib_base
    desktop-app::lib_ui
    desktop-app::external_zlib
    tdesktop::td_scheme
)

set_target_properties(test_ayu_secret PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

if (DESKTOP_APP_TEST_APPS)
    add_dependencies(Telegram test_ayu_secret)
endif()
