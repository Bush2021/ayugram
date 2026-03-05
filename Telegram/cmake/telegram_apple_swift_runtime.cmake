# This file is part of Telegram Desktop,
# the official desktop application for the Telegram messaging service.
#
# For license and copyright information please follow this link:
# https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL

function(telegram_add_apple_swift_runtime target_name)
    if (NOT APPLE)
        return()
    endif()

    execute_process(
        COMMAND xcrun --find swiftc
        OUTPUT_VARIABLE _swift_compiler_path
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if (NOT _swift_compiler_path)
        set(_swift_compiler_path "${CMAKE_Swift_COMPILER}")
    endif()

    get_filename_component(_swift_compiler_dir "${_swift_compiler_path}" DIRECTORY)
    get_filename_component(_swift_toolchain_dir "${_swift_compiler_dir}/../.." REALPATH)
    set(_swift_runtime_dir "${_swift_toolchain_dir}/usr/lib/swift")
    set(_swift_platform_runtime_dir "${_swift_runtime_dir}/macosx")

    target_link_directories(${target_name}
    PRIVATE
        "${_swift_platform_runtime_dir}"
        "${_swift_runtime_dir}"
    )

    target_link_options(${target_name}
    PRIVATE
        "-L${_swift_platform_runtime_dir}"
        "-L${_swift_runtime_dir}"
        "-Wl,-rpath,/usr/lib/swift"
        "-Wl,-rpath,@executable_path/../Frameworks"
    )

    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND mkdir -p $<TARGET_FILE_DIR:${target_name}>/../Frameworks
        COMMAND xcrun swift-stdlib-tool
            --copy
            --platform macosx
            --scan-executable $<TARGET_FILE:${target_name}>
            --destination $<TARGET_FILE_DIR:${target_name}>/../Frameworks
        VERBATIM
    )
endfunction()
