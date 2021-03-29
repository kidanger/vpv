set(IPO_SUPPORTED OFF)
if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.9.0")
    include(CheckIPOSupported)
    check_ipo_supported(RESULT IPO_SUPPORTED)
endif()

function(target_enable_ipo_if_supported target)
    if(IPO_SUPPORTED)
       set_target_properties(${target} PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
       set_target_properties(${target} PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)
       set_target_properties(${target} PROPERTIES INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL TRUE)
    endif()
endfunction()

