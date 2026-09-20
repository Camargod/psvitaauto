cmake_minimum_required(VERSION 3.16)
project(psvitaauto C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# mbedTLS — portable C, built for both host and Vita.
set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/mbedtls)

if(CMAKE_CROSSCOMPILING)
  target_compile_definitions(mbedtls PUBLIC MBEDTLS_NO_PLATFORM_ENTROPY MBEDTLS_PLATFORM_MS_TIME_ALT)
  target_compile_definitions(mbedcrypto PUBLIC MBEDTLS_NO_PLATFORM_ENTROPY MBEDTLS_PLATFORM_MS_TIME_ALT)
  target_compile_definitions(mbedx509 PUBLIC MBEDTLS_NO_PLATFORM_ENTROPY MBEDTLS_PLATFORM_MS_TIME_ALT)
endif()

# aa-core — portable; transport/discover use newlib sockets (available on Vita).
add_library(core STATIC
  src/core/core.c
  src/core/frame.c
  src/core/stream.c
  src/core/transport.c
  src/core/tls.c
  src/core/aa_cert.c
  src/core/aa_msg.c
  src/core/session.c
  src/core/version.c
  src/core/control.c
  src/core/discover.c
  third_party/nanopb/pb_common.c
  third_party/nanopb/pb_encode.c
  third_party/nanopb/pb_decode.c
)

file(GLOB_RECURSE AA_PROTO_SOURCES CONFIGURE_DEPENDS src/core/aa_proto_generated/*.c)
target_sources(core PRIVATE ${AA_PROTO_SOURCES})

target_include_directories(core PUBLIC
  src/core
  src/core/aa_proto_generated
  third_party/nanopb
)
target_compile_definitions(core PUBLIC PB_FIELD_32BIT)
target_link_libraries(core PUBLIC mbedtls mbedx509 mbedcrypto)

if(CMAKE_CROSSCOMPILING)
  include("${VITASDK}/share/vita.cmake")

  set(VITA_APP_NAME "PSVitaAuto")
  set(VITA_TITLEID "PSVTAUTO1")
  set(VITA_VERSION  "00.01")

  add_executable(psvitaauto src/vita/vita_video.c src/vita/avcdec.c)
  target_include_directories(psvitaauto PRIVATE src/vita)

  target_link_libraries(psvitaauto
    core
    vita2d
    debugnet
    SceDisplay_stub
    SceGxm_stub
    SceCtrl_stub
    SceTouch_stub
    SceLibKernel_stub
    SceSysmodule_stub
    SceAppMgr_stub
    SceNet_stub
    SceNetCtl_stub
    ScePgf_stub
    ScePvf_stub
    SceCommonDialog_stub
    SceVideodec_stub
    pthread
    m
  )

  vita_create_self(psvitaauto.self psvitaauto UNSAFE)
  vita_create_vpk(psvitaauto.vpk ${VITA_TITLEID} psvitaauto.self
    VERSION ${VITA_VERSION}
    NAME ${VITA_APP_NAME}
  )
else()
  add_library(hostsupport STATIC src/host/mock_phone.c)
  target_include_directories(hostsupport PUBLIC src/host)
  target_link_libraries(hostsupport PUBLIC core)

  add_executable(aa_harness src/host/harness.c)
  target_link_libraries(aa_harness core hostsupport)

  enable_testing()
  add_subdirectory(tests)
endif()
