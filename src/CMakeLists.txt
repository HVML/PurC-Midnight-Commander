# -----------------------------------------------------------------------------
# Add components directories
# -----------------------------------------------------------------------------
add_subdirectory(wtf)

add_subdirectory(lib)
add_subdirectory(bin)

if (ENABLE_API_TESTS)
#    add_subdirectory(tests)
endif ()

PURCMC_INCLUDE_CONFIG_FILES_IF_EXISTS()
