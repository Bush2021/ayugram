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
