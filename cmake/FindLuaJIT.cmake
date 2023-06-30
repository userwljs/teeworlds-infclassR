include(FindPackageHandleStandardArgs)
include(CMakeFindDependencyMacro)

find_dependency(PkgConfig)
pkg_check_modules(PC_LuaJIT QUIET luajit)

find_package_handle_standard_args(LuaJIT
  REQUIRED_VARS
    PC_LuaJIT_LIBRARIES
  VERSION_VAR
    PC_LuaJIT_VERSION
)

if(LuaJIT_FOUND)
  if(NOT TARGET LuaJIT::LuaJIT)
    add_library(LuaJIT::LuaJIT INTERFACE IMPORTED)
    if(PC_LuaJIT_INCLUDE_DIRS)
        set_property(TARGET LuaJIT::LuaJIT PROPERTY
            INTERFACE_INCLUDE_DIRECTORIES "${PC_LuaJIT_INCLUDE_DIRS}"
        )
    endif()
    set_property(TARGET LuaJIT::LuaJIT PROPERTY
        INTERFACE_LINK_LIBRARIES "${PC_LuaJIT_LIBRARIES}"
    )
  endif()
endif()
