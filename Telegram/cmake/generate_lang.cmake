# This file is part of Telegram Desktop,
# the official desktop application for the Telegram messaging service.
#
# For license and copyright information please follow this link:
# https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL

function(generate_lang target_name lang_file src_loc)
    set(gen_dst ${CMAKE_CURRENT_BINARY_DIR}/gen)
    file(MAKE_DIRECTORY ${gen_dst})

    set(gen_timestamp ${gen_dst}/lang_auto.timestamp)
    set(gen_keys ${gen_dst}/lang_auto_keys.h)
    set(gen_files
        ${gen_dst}/lang_auto.cpp
        ${gen_dst}/lang_auto.h
        ${gen_dst}/lang_auto_counts.h
        ${gen_keys}
    )

    add_custom_command(
    OUTPUT
        ${gen_timestamp}
    BYPRODUCTS
        ${gen_files}
    COMMAND
        codegen_lang
        -o${gen_dst}
        ${lang_file}
    COMMENT "Generating lang (${target_name})"
    DEPENDS
        codegen_lang
        ${lang_file}
    )
    generate_target(${target_name} lang ${gen_timestamp} "${gen_files}" ${gen_dst})

    file(GLOB_RECURSE lang_sources CONFIGURE_DEPENDS
        ${src_loc}/*.cpp
        ${src_loc}/*.h
        ${src_loc}/*.mm
    )

    # Xcode resolves precompiled headers through its SharedPrecompiledHeaders
    # cache, which is keyed on the full command line of each translation unit.
    # A per-source LANG_KEYS_SUBSET define makes every command line unique, so
    # the shared prefix header gets precompiled once per source file and fills
    # up the disk. Other generators precompile once per target and pass the
    # result with -include, so the define is free there. Without the define the
    # generated lang header falls back to the full keys header, which only
    # costs compile time.
    set(use_subset_defines TRUE)
    if (CMAKE_GENERATOR STREQUAL "Xcode")
        set(use_subset_defines FALSE)
    endif()

    set(subset_headers "")
    foreach (entry ${lang_sources})
        if (entry MATCHES "\\.(cpp|mm)$")
            file(RELATIVE_PATH relative ${src_loc} ${entry})
            list(APPEND subset_headers ${gen_dst}/lang_subsets/${relative}.h)
            if (use_subset_defines)
                set_property(SOURCE ${entry} APPEND PROPERTY COMPILE_DEFINITIONS
                    "LANG_KEYS_SUBSET=\"lang_subsets/${relative}.h\"")
            endif()
        endif()
    endforeach()

    set(subsets_timestamp ${gen_dst}/lang_subsets.timestamp)
    add_custom_command(
    OUTPUT
        ${subsets_timestamp}
        ${subset_headers}
    COMMAND
        codegen_lang
        --subsets-only
        -o${gen_dst}
        -s${src_loc}
        ${lang_file}
    COMMAND
        ${CMAKE_COMMAND} -E touch ${subsets_timestamp}
    COMMENT "Generating lang subsets (${target_name})"
    DEPENDS
        codegen_lang
        ${gen_keys}
        ${lang_sources}
    )
    add_custom_target(${target_name}_lang_subsets DEPENDS ${subsets_timestamp})
    init_target_folder(${target_name}_lang_subsets "(gen)")

    # The subsets command reads ${gen_keys}, which the lang command declares as
    # a BYPRODUCT. CMake only pulls a producing command into a consuming target
    # for files listed as its OUTPUT, so without this the Visual Studio
    # generator may run the subsets command before the keys header exists.
    add_dependencies(${target_name}_lang_subsets ${target_name}_lang)

    add_dependencies(${target_name} ${target_name}_lang_subsets)
endfunction()
