cmake_minimum_required(VERSION 3.22)
project(TidBank VERSION 0.1.0 LANGUAGES C CXX)  # C: JUCE requires it
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# dsp/ is header-only and JUCE-free: the ears harness and tests build without JUCE.
add_library(tidbank_dsp INTERFACE)
target_include_directories(tidbank_dsp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/dsp)

# Debug probe: per-sample delay tracing + CPU load, compiled out unless asked for. Never on in release builds.
option(TIDBANK_DEBUG_PROBE "Compile the debug probe (dsp/DebugProbe.h) into the engines, renderer and plugin" OFF)
if(TIDBANK_DEBUG_PROBE)
  target_compile_definitions(tidbank_dsp INTERFACE TIDBANK_DEBUG_PROBE)
endif()

add_executable(tidbank_render tools/ears/render.cpp)
target_link_libraries(tidbank_render PRIVATE tidbank_dsp)
target_compile_options(tidbank_render PRIVATE -Wall -Wextra)

# Same renderer with the probe compiled in, so the ears can test it whatever TIDBANK_DEBUG_PROBE is (renders identically).
add_executable(tidbank_render_probe tools/ears/render.cpp)
target_link_libraries(tidbank_render_probe PRIVATE tidbank_dsp)
target_compile_options(tidbank_render_probe PRIVATE -Wall -Wextra)
target_compile_definitions(tidbank_render_probe PRIVATE TIDBANK_DEBUG_PROBE)

enable_testing()
add_executable(tidbank_tests tests/dsp_tests.cpp)
target_link_libraries(tidbank_tests PRIVATE tidbank_dsp)
add_test(NAME dsp COMMAND tidbank_tests)
add_executable(tidbank_fb_tests tests/feedback_tests.cpp)
target_link_libraries(tidbank_fb_tests PRIVATE tidbank_dsp)
target_compile_options(tidbank_fb_tests PRIVATE -Wall -Wextra)
add_test(NAME feedback COMMAND tidbank_fb_tests)
add_executable(tidbank_bbd_tests tests/bbd_tests.cpp)
target_link_libraries(tidbank_bbd_tests PRIVATE tidbank_dsp)
target_compile_options(tidbank_bbd_tests PRIVATE -Wall -Wextra)
add_test(NAME bbd COMMAND tidbank_bbd_tests)
add_executable(tidbank_reel_tests tests/reel_tests.cpp)
target_link_libraries(tidbank_reel_tests PRIVATE tidbank_dsp)
target_compile_options(tidbank_reel_tests PRIVATE -Wall -Wextra)
add_test(NAME reel COMMAND tidbank_reel_tests)
add_executable(tidbank_filtaer_tests tests/filtaer_tests.cpp)
target_link_libraries(tidbank_filtaer_tests PRIVATE tidbank_dsp)
target_compile_options(tidbank_filtaer_tests PRIVATE -Wall -Wextra)
add_test(NAME filtaer COMMAND tidbank_filtaer_tests)

# Plugin wrapper (JUCE) is added once the dsp core is proven; see docs/design.md.
option(TIDBANK_BUILD_PLUGIN "Build the JUCE plugin" OFF)
# ON (default) copies the built VST3/AU into ~/Library/Audio/Plug-Ins and so REPLACES the installed plugin. Probe/test builds: OFF.
option(TIDBANK_COPY_PLUGIN "Copy the built plugin into ~/Library/Audio/Plug-Ins after the build" ON)
if(TIDBANK_BUILD_PLUGIN)
  # Pinned JUCE. Fetched only for the plugin build, so `make test`/`make ears` stay JUCE-free.
  include(FetchContent)
  FetchContent_Declare(JUCE
      GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
      GIT_TAG        d60b7a78030661574a615b586d9325ba7a7fa7dd  # 9.0.5
      GIT_SHALLOW    TRUE)
  FetchContent_MakeAvailable(JUCE)

  # BuildInfo.h (TIDBANK_BUILD = short commit hash) for the editors' status lines, regenerated on every build.
  set(TIDBANK_GEN_DIR "${CMAKE_BINARY_DIR}/generated")
  add_custom_target(tidbank_build_info
      COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR} -DOUT=${TIDBANK_GEN_DIR}/BuildInfo.h -P ${CMAKE_SOURCE_DIR}/cmake/build_info.cmake
      BYPRODUCTS ${TIDBANK_GEN_DIR}/BuildInfo.h
      COMMENT "BuildInfo.h")

  # Own manufacturer/plugin codes, so the TidBank plugins never overwrite each other or other plugins.
  juce_add_plugin(TidBank
      COMPANY_NAME "StigLau"
      BUNDLE_ID com.stiglau.tidbank
      IS_SYNTH FALSE
      NEEDS_MIDI_INPUT FALSE
      NEEDS_MIDI_OUTPUT FALSE
      IS_MIDI_EFFECT FALSE
      COPY_PLUGIN_AFTER_BUILD ${TIDBANK_COPY_PLUGIN}
      MICROPHONE_PERMISSION_ENABLED TRUE
      MICROPHONE_PERMISSION_TEXT "TidBank needs audio input to echo it."
      PLUGIN_MANUFACTURER_CODE Stlu
      PLUGIN_CODE Tdbk
      FORMATS VST3 AU Standalone
      PRODUCT_NAME "TidBank")
  juce_generate_juce_header(TidBank)
  target_sources(TidBank PRIVATE plugin/PluginProcessor.cpp plugin/PluginEditor.cpp)
  target_include_directories(TidBank PRIVATE plugin)
  target_compile_definitions(TidBank PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
  target_link_libraries(TidBank PRIVATE tidbank_dsp
      juce::juce_audio_utils
      juce::juce_recommended_config_flags
      juce::juce_recommended_lto_flags
      juce::juce_recommended_warning_flags)

  # Offscreen editor snapshot to PNG (agents look at the UI without screen-recording rights): make ui
  juce_add_console_app(tidbank_snapshot PRODUCT_NAME "tidbank_snapshot")
  juce_generate_juce_header(tidbank_snapshot)
  target_sources(tidbank_snapshot PRIVATE tools/ui/snapshot.cpp plugin/PluginProcessor.cpp plugin/PluginEditor.cpp)
  target_include_directories(tidbank_snapshot PRIVATE plugin)
  target_compile_definitions(tidbank_snapshot PRIVATE JucePlugin_Name="TidBank" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)  # modal: probe strip drain
  target_link_libraries(tidbank_snapshot PRIVATE tidbank_dsp juce::juce_audio_utils juce::juce_recommended_config_flags)

  # Host-free plugin checks (MIDI learn on every parameter, state round trip, bit-exact dry path): make plugin-test
  juce_add_console_app(tidbank_plugin_tests PRODUCT_NAME "tidbank_plugin_tests")
  juce_generate_juce_header(tidbank_plugin_tests)
  target_sources(tidbank_plugin_tests PRIVATE tests/plugin_tests.cpp plugin/PluginProcessor.cpp plugin/PluginEditor.cpp)
  target_include_directories(tidbank_plugin_tests PRIVATE plugin)
  target_compile_definitions(tidbank_plugin_tests PRIVATE JucePlugin_Name="TidBank" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
  target_link_libraries(tidbank_plugin_tests PRIVATE tidbank_dsp juce::juce_audio_utils juce::juce_recommended_config_flags)
  if(TIDBANK_DEBUG_PROBE)  # the probe test runs the message loop so the log Timer fires (MessageManager::runDispatchLoopUntil)
    target_compile_definitions(tidbank_plugin_tests PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)
  endif()

  # Reels (d26, d27): reel-only looper plugin, chained before TidBank in Live. Own codes (Stlu/Rels), never TidBank's.
  juce_add_plugin(Reels
      COMPANY_NAME "StigLau"
      BUNDLE_ID com.stiglau.reels
      IS_SYNTH FALSE
      NEEDS_MIDI_INPUT FALSE
      NEEDS_MIDI_OUTPUT FALSE
      IS_MIDI_EFFECT FALSE
      COPY_PLUGIN_AFTER_BUILD ${TIDBANK_COPY_PLUGIN}
      MICROPHONE_PERMISSION_ENABLED TRUE
      MICROPHONE_PERMISSION_TEXT "Reels needs audio input to record it."
      PLUGIN_MANUFACTURER_CODE Stlu
      PLUGIN_CODE Rels
      FORMATS VST3 AU Standalone
      PRODUCT_NAME "Reels")
  juce_generate_juce_header(Reels)
  target_sources(Reels PRIVATE reels/ReelsProcessor.cpp reels/ReelsEditor.cpp)
  target_include_directories(Reels PRIVATE reels)
  target_compile_definitions(Reels PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
  target_link_libraries(Reels PRIVATE tidbank_dsp
      juce::juce_audio_utils
      juce::juce_recommended_config_flags
      juce::juce_recommended_lto_flags
      juce::juce_recommended_warning_flags)
  juce_add_console_app(reels_plugin_tests PRODUCT_NAME "reels_plugin_tests")
  juce_generate_juce_header(reels_plugin_tests)
  target_sources(reels_plugin_tests PRIVATE tests/reels_plugin_tests.cpp reels/ReelsProcessor.cpp reels/ReelsEditor.cpp)
  target_include_directories(reels_plugin_tests PRIVATE reels)
  target_compile_definitions(reels_plugin_tests PRIVATE JucePlugin_Name="Reels" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
  target_link_libraries(reels_plugin_tests PRIVATE tidbank_dsp juce::juce_audio_utils juce::juce_recommended_config_flags)

  foreach(t TidBank tidbank_snapshot tidbank_plugin_tests Reels reels_plugin_tests)
    add_dependencies(${t} tidbank_build_info)
    target_include_directories(${t} PRIVATE ${TIDBANK_GEN_DIR})
  endforeach()

  # Filtær (d38): the Matriarch's two ladder filters as a stereo effect, before TidBank in Live. Own codes (Stlu/Fltr).
  # Names are ASCII: JUCE 9.0.5 double-encodes a UTF-8 PLUGIN_NAME ("Filtær" became "FiltÃ¦r") and the VST3 then failed to load
  # in pluginval (2026-11-18). The editor shows "Filtær".
  juce_add_plugin(Filtaer
      COMPANY_NAME "StigLau"
      BUNDLE_ID com.stiglau.filtaer
      IS_SYNTH FALSE
      NEEDS_MIDI_INPUT FALSE
      NEEDS_MIDI_OUTPUT FALSE
      IS_MIDI_EFFECT FALSE
      COPY_PLUGIN_AFTER_BUILD ${TIDBANK_COPY_PLUGIN}
      MICROPHONE_PERMISSION_ENABLED TRUE
      MICROPHONE_PERMISSION_TEXT "Filtær needs audio input to filter it."
      PLUGIN_MANUFACTURER_CODE Stlu
      PLUGIN_CODE Fltr
      FORMATS VST3 AU Standalone
      PRODUCT_NAME "Filtaer")
  juce_generate_juce_header(Filtaer)
  target_sources(Filtaer PRIVATE filtaer/FiltaerProcessor.cpp filtaer/FiltaerEditor.cpp)
  target_include_directories(Filtaer PRIVATE filtaer)
  target_compile_definitions(Filtaer PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
  target_link_libraries(Filtaer PRIVATE tidbank_dsp
      juce::juce_audio_utils
      juce::juce_recommended_config_flags
      juce::juce_recommended_lto_flags
      juce::juce_recommended_warning_flags)
  juce_add_console_app(filtaer_plugin_tests PRODUCT_NAME "filtaer_plugin_tests")
  juce_generate_juce_header(filtaer_plugin_tests)
  target_sources(filtaer_plugin_tests PRIVATE tests/filtaer_plugin_tests.cpp filtaer/FiltaerProcessor.cpp filtaer/FiltaerEditor.cpp)
  target_include_directories(filtaer_plugin_tests PRIVATE filtaer)
  target_compile_definitions(filtaer_plugin_tests PRIVATE JucePlugin_Name="Filtaer" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
  target_link_libraries(filtaer_plugin_tests PRIVATE tidbank_dsp juce::juce_audio_utils juce::juce_recommended_config_flags)

  # Virtual MIDI device "TidBank Test": CC sender to test host mapping without hardware: make midi-sweep
  juce_add_console_app(tidbank_midisend PRODUCT_NAME "tidbank_midisend")
  juce_generate_juce_header(tidbank_midisend)
  target_sources(tidbank_midisend PRIVATE tools/midi/midisend.cpp)
  target_compile_definitions(tidbank_midisend PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
  target_link_libraries(tidbank_midisend PRIVATE juce::juce_audio_devices juce::juce_recommended_config_flags)
endif()
