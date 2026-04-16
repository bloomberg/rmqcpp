# Bridge module: bde's bdlConfig.cmake expects find_package(libpcre2-8),
# but vcpkg's pcre2 port provides find_package(pcre2) with target PCRE2::8BIT.
find_package(pcre2 CONFIG REQUIRED)

if (TARGET PCRE2::8BIT AND NOT TARGET libpcre2-8::libpcre2-8)
    # Resolve PCRE2::8BIT to its underlying IMPORTED target before aliasing,
    # matching the approach used by the BDE vcpkg port.
    get_target_property(_pcre2_actual PCRE2::8BIT ALIASED_TARGET)
    if (_pcre2_actual)
        add_library(libpcre2-8::libpcre2-8 ALIAS "${_pcre2_actual}")
    else()
        add_library(libpcre2-8::libpcre2-8 ALIAS PCRE2::8BIT)
    endif()
endif()

set(libpcre2-8_FOUND TRUE)
