include(CheckCXXCompilerFlag)

function(add_cxx_flag_if_supported flags flag has)
    check_cxx_compiler_flag("${flag}" ${has}_CXX)
    if (${${has}_CXX})
        set(${has} TRUE PARENT_SCOPE)
        set(${flags} "${${flags}} ${flag}" PARENT_SCOPE)
    endif()
endfunction()

