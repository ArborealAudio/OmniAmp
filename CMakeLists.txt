cmake_minimum_required(VERSION 3.15)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_OSX_DEPLOYMENT_TARGET 10.9 CACHE INTERNAL "")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (PRODUCTION_BUILD)
    if (WIN32)
        set(CMAKE_SYSTEM_VERSION 7 CACHE INTERNAL "")
    elseif(APPLE)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE INTERNAL "")
    endif()
    set(CMAKE_BUILD_TYPE Release CACHE INTERNAL "")
else()
    set(PRODUCTION_BUILD OFF)
endif()
message("PRODUCTION_BUILD: ${PRODUCTION_BUILD}")

project(OmniAmp VERSION 1.0.3)

add_subdirectory(modules/juce)
add_subdirectory(modules/clap-juce-extensions EXCLUDE_FROM_ALL)

set(JUCE_FORMATS AU VST3)

if(BUILD_VST2)
    message("Adding VST2 target")
    juce_set_vst2_sdk_path("vstsdk2.4")
    list(APPEND JUCE_FORMATS VST)
endif()

if (NOT INSTALL)
    set(INSTALL OFF)
endif()

if (NOT NO_LICENSE_CHECK)
    set(NO_LICENSE_CHECK 0)
endif()

juce_add_plugin(OmniAmp
    COMPANY_NAME "Arboreal Audio"
    COMPANY_COPYRIGHT "(c) 2026 Arboreal Audio, LLC"
    COPY_PLUGIN_AFTER_BUILD ${INSTALL}
    PLUGIN_MANUFACTURER_CODE Arau
    PLUGIN_CODE Oamp
    BUNDLE_ID "com.ArborealAudio.OmniAmp"
    FORMATS ${JUCE_FORMATS}
    VST2_CATEGORY kPlugCategEffect
    VST3_CATEGORY Fx Distortion
    AU_MAIN_TYPE kAudioUnitType_Effect
    PRODUCT_NAME "OmniAmp"
	NEEDS_CURL FALSE
    HARDENED_RUNTIME_ENABLE TRUE
    LV2URI https://github.com/ArborealAudio/OmniAmp.git
)

clap_juce_extensions_plugin(TARGET OmniAmp
    CLAP_ID "com.ArborealAudio.OmniAmp.clap"
    CLAP_PROCESS_EVENTS_RESOLUTION_SAMPLES 32
    CLAP_FEATURES audio-effect distortion reverb EQ)

if (NOT BETA_BUILD)
    set(BETA_BUILD OFF)
endif()
message("BETA_BUILD: ${BETA_BUILD}")

juce_generate_juce_header(OmniAmp)

target_sources(OmniAmp
    PRIVATE
        Source/PluginEditor.cpp
        Source/PluginProcessor.cpp
        Source/Activation.hpp)

add_subdirectory(Source/Processors)
add_subdirectory(Source/UI)
target_include_directories(OmniAmp PRIVATE
    "modules/chowdsp_wdf/include"
    "modules/Arbor_modules/")

juce_add_modules(
    "modules/gin/modules/gin"
    "modules/gin/modules/gin_graphics"
    "modules/Arbor_modules")

include(ExternalProject)

# OpenSSL
set(OPENSSL_INSTALL_DIR "${CMAKE_BINARY_DIR}/deps/openssl")
message("OpenSSL install dir: ${OPENSSL_INSTALL_DIR}")
ExternalProject_Add(dep_openssl
	SOURCE_DIR
		"${CMAKE_CURRENT_SOURCE_DIR}/modules/openssl"
	PREFIX "${OPENSSL_INSTALL_DIR}"
	CONFIGURE_COMMAND
		<SOURCE_DIR>/Configure
		--prefix=<INSTALL_DIR>
		--openssldir="${OPENSSL_INSTALL_DIR}/ssl"
		--libdir=lib
		no-shared
		no-tests
		no-apps
	BUILD_COMMAND
		$(MAKE)
	INSTALL_COMMAND
		$(MAKE) install_sw
	BUILD_IN_SOURCE
		TRUE
	BUILD_BYPRODUCTS
		${OPENSSL_INSTALL_DIR}/lib/libssl.a
		${OPENSSL_INSTALL_DIR}/lib/libcrypto.a
)
add_library(OpenSSL::SSL STATIC IMPORTED)
set_target_properties(OpenSSL::SSL PROPERTIES
	IMPORTED_LOCATION ${OPENSSL_INSTALL_DIR}/lib/libssl.a
	INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/modules/openssl/include"
)
add_dependencies(OpenSSL::SSL dep_openssl)
add_library(OpenSSL::Crypto STATIC IMPORTED)
set_target_properties(OpenSSL::Crypto PROPERTIES
	IMPORTED_LOCATION ${OPENSSL_INSTALL_DIR}/lib/libcrypto.a
	INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/modules/openssl/include"
)
add_dependencies(OpenSSL::Crypto dep_openssl)


# CURL
set(CURL_INSTALL_DIR "${CMAKE_BINARY_DIR}/deps/curl")
message("Curl install dir: ${CURL_INSTALL_DIR}")
ExternalProject_Add(dep_curl
    SOURCE_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/modules/curl"
	PREFIX
		"${CURL_INSTALL_DIR}"
    DEPENDS
        dep_openssl

    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${CURL_INSTALL_DIR}
        -DBUILD_SHARED_LIBS=OFF
        -DBUILD_CURL_EXE=OFF
        -DBUILD_TESTING=OFF
		-DCURL_STATICLIB=ON

        -DCURL_USE_OPENSSL=ON
        -DOPENSSL_USE_STATIC_LIBS=ON
        -DOPENSSL_ROOT_DIR=${OPENSSL_INSTALL_DIR}

        -DCURL_ZLIB=OFF
        -DCURL_BROTLI=OFF
        -DCURL_ZSTD=OFF
        -DCURL_USE_LIBPSL=OFF
        -DCURL_USE_LIBIDN2=OFF
        -DCURL_USE_LIBSSH=OFF
        -DCURL_USE_LIBSSH2=OFF
        -DCURL_USE_GSSAPI=OFF
        -DCURL_USE_GSASL=OFF

    BUILD_COMMAND
        ${CMAKE_COMMAND} --build .

    INSTALL_COMMAND
        ${CMAKE_COMMAND} --install .
	
	BUILD_BYPRODUCTS
		${CURL_INSTALL_DIR}/lib/libcurl.a
)
add_library(CURL::libcurl STATIC IMPORTED)
set_target_properties(CURL::libcurl PROPERTIES
	IMPORTED_LOCATION "${CURL_INSTALL_DIR}/lib/libcurl.a"
	INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/modules/curl/include"
)
add_dependencies(CURL::libcurl dep_curl)

target_compile_definitions(OmniAmp
    PUBLIC
        PRODUCTION_BUILD=${PRODUCTION_BUILD}
        BETA_BUILD=${BETA_BUILD}
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_COREGRAPHICS_RENDER_WITH_MULTIPLE_PAINT_CALLS=1
        AWS_API_KEY="$ENV{AWS_API_KEY}"
        NO_LICENSE_CHECK=${NO_LICENSE_CHECK}
)

juce_add_binary_data(BinaryData SOURCES
    "Resources/2x12.svg"
    "Resources/4x12.svg"
    "Resources/6x10.svg"
    "Resources/mic.svg"
    "Resources/speaker.svg"
    "Resources/Link/link.svg"
    "Resources/Link/link_off.svg"
    "Resources/Link/link_over_off.svg"
    "Resources/Link/link_over_on.svg"
    "Resources/logo.svg"
    "Resources/Sora-Regular.ttf"
    "Resources/Hamburger_icon.svg"
)

# -fPIC flag for Linux
if (UNIX AND NOT APPLE)
    set_target_properties(BinaryData PROPERTIES
        POSITION_INDEPENDENT_CODE TRUE)
    target_link_libraries(OmniAmp PRIVATE "-static-libgcc -static-libstdc++")
endif()

target_link_libraries(OmniAmp
    PRIVATE
        BinaryData
        juce::juce_audio_utils
        juce::juce_dsp
        gin
        gin_graphics
		CURL::libcurl
		OpenSSL::SSL
		OpenSSL::Crypto
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

if (NOT APPLE)
target_link_libraries(OmniAmp
    PRIVATE
    juce::juce_opengl)
endif()
