cmake_minimum_required(VERSION 3.28)
project(BridgedCvGameCore LANGUAGES CXX ASM)
set(CMAKE_CXX_STANDARD_LIBRARIES "")
set(CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS "")
set(CMAKE_CXX_USING_LINKER_DEFAULT "")
set(CMAKE_CXX_CREATE_SHARED_LIBRARY
  "<CMAKE_LINKER> /dll /machine:x86 /nodefaultlib /timestamp:0 /entry:_DllMainCRTStartup@12 /out:<TARGET> /implib:<TARGET_IMPLIB> <LINK_FLAGS> <OBJECTS> <LINK_LIBRARIES>")

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR NOT CMAKE_CXX_COMPILER_VERSION VERSION_EQUAL "23.1.2"
   OR NOT CMAKE_CXX_COMPILER_TARGET STREQUAL "i686-pc-windows-msvc")
  message(FATAL_ERROR "Use cmake/windows-x86-clang.cmake with pinned Clang 23.1.2")
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG" CACHE STRING "Pinned serial release optimization" FORCE)
set(BRIDGED_VC71 "" CACHE PATH "Acquired Microsoft Visual C++ Toolkit 2003 root")
set(BRIDGED_WINDOWS_SDK "" CACHE PATH "Acquired WindowsSDK root")
foreach(input IN ITEMS BRIDGED_VC71 BRIDGED_WINDOWS_SDK)
  if(NOT EXISTS "${${input}}")
    message(FATAL_ERROR "Set ${input} to the acquired pinned target input")
  endif()
endforeach()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(compat_dir "${CMAKE_CURRENT_BINARY_DIR}/compat-headers")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/prepare_headers.py"
  --vc71 "${BRIDGED_VC71}" --sdk "${BRIDGED_WINDOWS_SDK}" --output "${compat_dir}"
  RESULT_VARIABLE header_result OUTPUT_VARIABLE header_output ERROR_VARIABLE header_error)
if(NOT header_result EQUAL 0)
  message(FATAL_ERROR "Target header preparation failed: ${header_error}")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/tools/prepare_headers.py"
  "${CMAKE_CURRENT_SOURCE_DIR}/tools/build_support.py"
  "${CMAKE_CURRENT_SOURCE_DIR}/tools/dependency-lock.json")

# The bounded adapter translates Clang's EH3 frames through the installed
# VC7.1 exception machinery. Keep its strict C++ and x86 assembly separate
# from the inherited SDK translation units and their compatibility flags.
add_library(msvc_eh OBJECT compat/msvc_eh.cpp compat/msvc_eh_x86.s)
set_target_properties(msvc_eh PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED YES
  CXX_EXTENSIONS NO)
target_compile_options(msvc_eh PRIVATE
  "$<$<COMPILE_LANGUAGE:CXX>:-std=c++23;-pedantic-errors;-Wall;-Wextra;-Werror;-ffreestanding;-fno-builtin;-fno-exceptions;-fno-rtti;-fno-stack-protector;-fms-extensions>"
  "$<$<COMPILE_LANGUAGE:ASM>:--target=i686-pc-windows-msvc>")

# Match the inherited Makefile's direct .cpp inventory. No nested Boost runtime
# source is built, and the inherited legacy build files remain references.
file(GLOB sdk_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/CvGameCoreDLL/*.cpp")
add_library(CvGameCoreDLL SHARED ${sdk_sources} $<TARGET_OBJECTS:msvc_eh>)
set_target_properties(CvGameCoreDLL PROPERTIES PREFIX "" SUFFIX ".dll"
  IMPORT_PREFIX "" IMPORT_SUFFIX ".lib" CXX_STANDARD 23 CXX_STANDARD_REQUIRED YES
  CXX_EXTENSIONS NO)
target_compile_definitions(CvGameCoreDLL PRIVATE WIN32 _WINDOWS _USRDLL
  CVGAMECOREDLL_EXPORTS NDEBUG FINAL_RELEASE _MT _DLL)
# Optional profiling build: times the SDK's existing PROFILE scopes inside the DLL and
# writes them to the file named by the BRIDGED_PROFILE_LOG environment variable.
option(BRIDGED_PROFILE "Time the SDK PROFILE scopes inside the DLL" OFF)
if(BRIDGED_PROFILE)
  target_compile_definitions(CvGameCoreDLL PRIVATE BRIDGED_PROFILE)
endif()
# How far the DLL may depart from the stock game (CvGameCoreDLL/BridgedTiers.h):
# 1 reproduces BtS, 2 may play a different game but always the same one, 3 may play a
# different game from one run to the next.
set(BRIDGED_TIER "2" CACHE STRING "Tiers of change the DLL has: 1, 2 or 3")
if(BRIDGED_TIER STREQUAL "1")
  target_compile_definitions(CvGameCoreDLL PRIVATE REPRODUCE_BTS)
elseif(BRIDGED_TIER STREQUAL "3")
  target_compile_definitions(CvGameCoreDLL PRIVATE NON_DETERMINISM_OK)
elseif(NOT BRIDGED_TIER STREQUAL "2")
  message(FATAL_ERROR "BRIDGED_TIER must be 1, 2 or 3")
endif()
target_include_directories(CvGameCoreDLL PRIVATE "${compat_dir}" "${compat_dir}/include-2")
target_include_directories(CvGameCoreDLL SYSTEM PRIVATE
  "${compat_dir}/include-2/Boost-1.32.0/include"
  "${compat_dir}/include-2/Python24/include"
  "${compat_dir}/include-0" "${compat_dir}/include-1")
target_compile_options(CvGameCoreDLL PRIVATE
  -ivfsoverlay "${compat_dir}/overlay.json"
  -include "${CMAKE_CURRENT_SOURCE_DIR}/compat/vc71_prelude.h"
  -fms-extensions -fms-compatibility-version=13.10
  -fno-sized-deallocation -fno-threadsafe-statics -fcheck-new
  -Xclang -fno-wchar -fno-stack-protector
  -fno-strict-aliasing -ffp-contract=off -Wall -Wextra)
target_link_options(CvGameCoreDLL PRIVATE /subsystem:windows /largeaddressaware /opt:ref /opt:icf)
# Link-time optimisation of the SDK units. Without it no call into another source file is
# ever inlined, and nearly every accessor in this code is a one-line function in a .cpp
# file. The EH adapter stays ordinary object code. The linker keeps the one object it makes
# of the SDK (CvGameCoreDLL.dll.lto.obj) so that tools/build_dll.py can check its exception
# metadata, as it checks each object of a build without this.
option(BRIDGED_LTO "Optimise the SDK across source files at link time" ON)
if(BRIDGED_LTO)
  target_compile_options(CvGameCoreDLL PRIVATE -flto)
  target_link_options(CvGameCoreDLL PRIVATE /lldsavetemps:prelink)
endif()
# Profile-guided optimisation. TRAIN builds a DLL that counts how often each block of code
# runs and writes the counts to the file named by the BRIDGED_PGO_RAW environment variable
# (see CvGameCoreDLL.cpp). USE optimises with a profile made from such counts by
# llvm-profdata merge.
set(BRIDGED_PGO "OFF" CACHE STRING "Profile-guided optimisation: OFF, TRAIN or USE")
set(BRIDGED_PGO_PROFILE "" CACHE FILEPATH "Merged profile (.profdata) for BRIDGED_PGO=USE")
if(BRIDGED_PGO STREQUAL "TRAIN")
  target_compile_definitions(CvGameCoreDLL PRIVATE BRIDGED_PGO_TRAIN)
  target_compile_options(CvGameCoreDLL PRIVATE -fprofile-generate)
elseif(BRIDGED_PGO STREQUAL "USE")
  if(NOT EXISTS "${BRIDGED_PGO_PROFILE}")
    message(FATAL_ERROR "Set BRIDGED_PGO_PROFILE to the merged profile")
  endif()
  target_compile_options(CvGameCoreDLL PRIVATE "-fprofile-use=${BRIDGED_PGO_PROFILE}"
    -Wno-profile-instr-unprofiled -Wno-profile-instr-out-of-date)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${BRIDGED_PGO_PROFILE}")
  set_property(TARGET CvGameCoreDLL APPEND PROPERTY OBJECT_DEPENDS "${BRIDGED_PGO_PROFILE}")
elseif(NOT BRIDGED_PGO STREQUAL "OFF")
  message(FATAL_ERROR "BRIDGED_PGO must be OFF, TRAIN or USE")
endif()
target_link_libraries(CvGameCoreDLL PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/CvGameCoreDLL/Python24/libs/python24.lib"
  "${CMAKE_CURRENT_SOURCE_DIR}/CvGameCoreDLL/Boost-1.32.0/libs/boost_python-vc71-mt-1_32.lib"
  "${BRIDGED_VC71}/lib/msvcrt.lib" "${BRIDGED_VC71}/lib/msvcprt.lib"
  "${BRIDGED_VC71}/lib/oldnames.lib"
  "${BRIDGED_WINDOWS_SDK}/lib/Kernel32.Lib" "${BRIDGED_WINDOWS_SDK}/lib/User32.Lib"
  "${BRIDGED_WINDOWS_SDK}/lib/WinMM.Lib")
