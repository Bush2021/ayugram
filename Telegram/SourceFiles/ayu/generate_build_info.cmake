if (NOT DEFINED source_dir OR NOT DEFINED output_file)
    message(FATAL_ERROR "Build info paths are required.")
endif()

find_package(Git QUIET)

set(commit)
if (Git_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE}
            -C ${source_dir}
            rev-parse
            --short=7
            HEAD
        RESULT_VARIABLE result
        OUTPUT_VARIABLE commit
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    string(LENGTH "${commit}" length)
    if (NOT result EQUAL 0
        OR length LESS 7
        OR NOT commit MATCHES "^[0-9a-f]+$")
        set(commit)
    endif()
endif()

set(content
    "#pragma once\n\ninline constexpr auto AyuBuildCommit = \"${commit}\";\n")
get_filename_component(output_dir "${output_file}" DIRECTORY)
file(MAKE_DIRECTORY "${output_dir}")

set(previous)
if (EXISTS "${output_file}")
    file(READ "${output_file}" previous)
endif()
if (NOT "${previous}" STREQUAL "${content}")
    file(WRITE "${output_file}" "${content}")
endif()
