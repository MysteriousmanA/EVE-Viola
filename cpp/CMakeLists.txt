add_executable(cpp_native_import_test
    tests/native_import_test.cpp
)

set_target_properties(cpp_native_import_test PROPERTIES
    AUTOMOC ON
)

target_compile_features(cpp_native_import_test PRIVATE cxx_std_20)
target_link_libraries(cpp_native_import_test PRIVATE
    eve_viola_native
    Qt6::Core
    Qt6::Test
)

add_test(NAME cpp_native_import_test COMMAND cpp_native_import_test)
set_tests_properties(cpp_native_import_test PROPERTIES
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)

add_library(eve_viola_domain STATIC
    src/features/replica_list/statistics_workload_estimator.h
    src/features/replica_list/statistics_workload_estimator.cpp
    src/features/page_partition/page_partition_model.h
    src/features/page_partition/page_partition_model.cpp
    src/features/desktop_pet/desktop_pet_settings_store.h
    src/features/desktop_pet/desktop_pet_settings_store.cpp
    src/features/expedition_timing/expedition_timing_service.h
    src/features/expedition_timing/expedition_timing_service.cpp
    src/config/config_types.h
    src/config/json_config_store.h
    src/features/quick_switch/quick_switch_keys.h
    src/features/quick_switch/quick_switch_keys.cpp
    src/features/quick_switch/radial_account_order.h
    src/features/quick_switch/radial_account_order.cpp
    src/config/config_validation.h
    src/config/config_validation.cpp
    src/features/color_rule/trigger_condition.h
    src/features/color_rule/trigger_condition.cpp
    src/features/color_rule/color_rule_config.h
    src/features/color_rule/color_rule_config.cpp
    src/features/region_preset/region_preset_config.h
    src/features/region_preset/region_preset_config.cpp
    src/config/serialization/region_presets_json.h
    src/config/serialization/region_presets_json.cpp
    src/config/serialization/color_rules_json.h
    src/config/serialization/color_rules_json.cpp
    src/config/serialization/page_partition_json.h
    src/config/serialization/page_partition_json.cpp
    src/config/serialization/replicas_json.h
    src/config/serialization/replicas_json.cpp
    src/config/serialization/presets_json.h
    src/config/serialization/presets_json.cpp
    src/config/serialization/quick_switch_json.h
    src/config/serialization/quick_switch_json.cpp
    src/config/json_document_io.h
    src/config/json_document_io.cpp
    src/config/serialization/application_options_json.h
    src/config/serialization/application_options_json.cpp
    src/config/json_config_store.cpp
    src/config/app_config.h
    src/features/color_rule/color_logic_engine.cpp
    src/features/color_rule/color_logic_engine.h
    src/features/replica/pixel_statistics.cpp
    src/features/replica/pixel_statistics.h
    src/features/replica/pixel_statistics_calculator.cpp
    src/features/replica/pixel_statistics_calculator.h
    src/features/replica/replica_manager.cpp
    src/features/replica/replica_workspace.cpp
    src/features/replica/replica_workspace.h
    src/features/replica/replica_controller.cpp
    src/features/replica/replica_controller.h
    src/runtime/runtime_policy.h
)
target_compile_features(eve_viola_domain PUBLIC cxx_std_20)
target_include_directories(eve_viola_domain PUBLIC src)
target_link_libraries(eve_viola_domain PUBLIC Qt6::Core Qt6::Gui)

add_executable(cpp_config_modules_test tests/config_modules_test.cpp)
set_target_properties(cpp_config_modules_test PROPERTIES AUTOMOC ON)
target_link_libraries(cpp_config_modules_test PRIVATE eve_viola_domain Qt6::Test)
add_test(NAME cpp_config_modules_test COMMAND cpp_config_modules_test)
set_tests_properties(cpp_config_modules_test PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)

add_executable(cpp_page_feature_services_test tests/page_feature_services_test.cpp)
set_target_properties(cpp_page_feature_services_test PROPERTIES AUTOMOC ON)
target_link_libraries(cpp_page_feature_services_test PRIVATE eve_viola_domain Qt6::Test)
add_test(NAME cpp_page_feature_services_test COMMAND cpp_page_feature_services_test)
set_tests_properties(cpp_page_feature_services_test PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)

add_library(eve_viola_cpp_core STATIC
    src/app/activation_coordinator.cpp
    src/app/activation_coordinator.h
    src/features/region_preset/preset_service.cpp
    src/features/region_preset/preset_service.h
    src/runtime/runtime_policy.h
    src/platform/window_catalog.cpp
    src/platform/window_catalog.h
    src/platform/native_gateway.cpp
    src/runtime/performance_recorder.cpp
    src/runtime/performance_recorder.h
    src/features/quick_switch/quick_switch_service.cpp
    src/features/quick_switch/quick_switch_service_impl.h
    src/features/quick_switch/quick_switch_key_mapping.cpp
    src/features/quick_switch/quick_switch_capture.cpp
    src/features/quick_switch/quick_switch_signal_dispatch.cpp
    src/features/quick_switch/quick_switch_foreground.cpp
    src/features/quick_switch/quick_switch_input_dispatch.cpp
    src/features/quick_switch/quick_switch_keyboard_input.cpp
    src/features/quick_switch/quick_switch_radial_hold.cpp
    src/features/quick_switch/quick_switch_mouse_input.cpp
    src/features/quick_switch/quick_switch_hook_thread.cpp
    src/features/quick_switch/quick_switch_service.h
    src/runtime/runtime_logger.cpp
    src/runtime/runtime_logger.h
    src/features/replica/statistics_coordinator.cpp
    src/features/replica/statistics_coordinator.h
)
set_target_properties(eve_viola_cpp_core PROPERTIES AUTOMOC ON)
target_compile_features(eve_viola_cpp_core PUBLIC cxx_std_20)
target_include_directories(eve_viola_cpp_core PUBLIC src)
target_link_libraries(eve_viola_cpp_core PUBLIC
    eve_viola_domain
    eve_viola_native
    Qt6::Concurrent
    Qt6::Core
    Qt6::Gui
)
if(WIN32)
    target_link_libraries(eve_viola_cpp_core PRIVATE user32)
endif()

add_executable(cpp_color_logic_engine_test tests/color_logic_engine_test.cpp)

add_executable(cpp_quick_switch_service_test tests/quick_switch_service_test.cpp)
set_target_properties(cpp_quick_switch_service_test PROPERTIES AUTOMOC ON)
target_link_libraries(cpp_quick_switch_service_test PRIVATE eve_viola_cpp_core Qt6::Test)
add_test(NAME cpp_quick_switch_service_test COMMAND cpp_quick_switch_service_test)
set_tests_properties(cpp_quick_switch_service_test PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)

add_executable(cpp_architecture_services_test tests/architecture_services_test.cpp)
set_target_properties(cpp_architecture_services_test PROPERTIES AUTOMOC ON)
target_link_libraries(cpp_architecture_services_test PRIVATE eve_viola_cpp_core Qt6::Test)
add_test(NAME cpp_architecture_services_test COMMAND cpp_architecture_services_test)
set_tests_properties(cpp_architecture_services_test PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)
set_target_properties(cpp_color_logic_engine_test PROPERTIES AUTOMOC ON)
target_compile_features(cpp_color_logic_engine_test PRIVATE cxx_std_20)
target_include_directories(cpp_color_logic_engine_test PRIVATE src)
target_link_libraries(cpp_color_logic_engine_test PRIVATE eve_viola_cpp_core Qt6::Test)

add_test(NAME cpp_color_logic_engine_test COMMAND cpp_color_logic_engine_test)
set_tests_properties(cpp_color_logic_engine_test PROPERTIES
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)

add_executable(cpp_native_gateway_test tests/native_gateway_test.cpp)

set_target_properties(cpp_native_gateway_test PROPERTIES AUTOMOC ON)
target_compile_features(cpp_native_gateway_test PRIVATE cxx_std_20)
target_include_directories(cpp_native_gateway_test PRIVATE src)
target_link_libraries(cpp_native_gateway_test PRIVATE
    eve_viola_cpp_core
    Qt6::Test
)

add_test(NAME cpp_native_gateway_test COMMAND cpp_native_gateway_test)
set_tests_properties(cpp_native_gateway_test PROPERTIES
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)

add_executable(cpp_pixel_statistics_test tests/pixel_statistics_test.cpp)
set_target_properties(cpp_pixel_statistics_test PROPERTIES AUTOMOC ON)
target_compile_features(cpp_pixel_statistics_test PRIVATE cxx_std_20)
target_include_directories(cpp_pixel_statistics_test PRIVATE src)
target_link_libraries(cpp_pixel_statistics_test PRIVATE eve_viola_cpp_core Qt6::Test)

add_test(NAME cpp_pixel_statistics_test COMMAND cpp_pixel_statistics_test)
set_tests_properties(cpp_pixel_statistics_test PROPERTIES
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)

add_executable(cpp_statistics_coordinator_test tests/statistics_coordinator_test.cpp)
set_target_properties(cpp_statistics_coordinator_test PROPERTIES AUTOMOC ON)
target_compile_features(cpp_statistics_coordinator_test PRIVATE cxx_std_20)
target_include_directories(cpp_statistics_coordinator_test PRIVATE src)
target_link_libraries(cpp_statistics_coordinator_test PRIVATE eve_viola_cpp_core Qt6::Test)

add_test(NAME cpp_statistics_coordinator_test COMMAND cpp_statistics_coordinator_test)
set_tests_properties(cpp_statistics_coordinator_test PROPERTIES
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)

add_executable(cpp_performance_recorder_test tests/performance_recorder_test.cpp)
set_target_properties(cpp_performance_recorder_test PROPERTIES AUTOMOC ON)
target_link_libraries(cpp_performance_recorder_test PRIVATE eve_viola_cpp_core Qt6::Test)
add_test(NAME cpp_performance_recorder_test COMMAND cpp_performance_recorder_test)
set_tests_properties(cpp_performance_recorder_test PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)

add_executable(eve_viola_performance_probe tools/performance_probe.cpp)
target_link_libraries(eve_viola_performance_probe PRIVATE eve_viola_cpp_core user32)

add_executable(cpp_audio_feedback_manager_test tests/audio_feedback_manager_test.cpp)
set_target_properties(cpp_audio_feedback_manager_test PROPERTIES AUTOMOC ON)
target_compile_features(cpp_audio_feedback_manager_test PRIVATE cxx_std_20)
target_include_directories(cpp_audio_feedback_manager_test PRIVATE src)
target_link_libraries(cpp_audio_feedback_manager_test PRIVATE eve_viola_cpp_ui Qt6::Test)

add_test(NAME cpp_audio_feedback_manager_test COMMAND cpp_audio_feedback_manager_test)
set_tests_properties(cpp_audio_feedback_manager_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)

add_executable(cpp_replica_manager_test tests/replica_manager_test.cpp)
set_target_properties(cpp_replica_manager_test PROPERTIES AUTOMOC ON)
target_compile_features(cpp_replica_manager_test PRIVATE cxx_std_20)
target_include_directories(cpp_replica_manager_test PRIVATE src)
target_link_libraries(cpp_replica_manager_test PRIVATE eve_viola_cpp_core Qt6::Test)

add_test(NAME cpp_replica_manager_test COMMAND cpp_replica_manager_test)
set_tests_properties(cpp_replica_manager_test PROPERTIES
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)

add_executable(cpp_replica_workspace_test tests/replica_workspace_test.cpp)
set_target_properties(cpp_replica_workspace_test PROPERTIES AUTOMOC ON)
target_compile_features(cpp_replica_workspace_test PRIVATE cxx_std_20)
target_include_directories(cpp_replica_workspace_test PRIVATE src)
target_link_libraries(cpp_replica_workspace_test PRIVATE eve_viola_cpp_core Qt6::Test)

add_test(NAME cpp_replica_workspace_test COMMAND cpp_replica_workspace_test)
set_tests_properties(cpp_replica_workspace_test PROPERTIES
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)

add_executable(cpp_app_config_test tests/app_config_test.cpp)
set_target_properties(cpp_app_config_test PROPERTIES AUTOMOC ON)
target_compile_features(cpp_app_config_test PRIVATE cxx_std_20)
target_include_directories(cpp_app_config_test PRIVATE src)
target_link_libraries(cpp_app_config_test PRIVATE eve_viola_cpp_core Qt6::Test)

add_test(NAME cpp_app_config_test COMMAND cpp_app_config_test)
set_tests_properties(cpp_app_config_test PROPERTIES
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)

add_library(eve_viola_cpp_ui STATIC
    src/ui/pages/quick_switch/quick_switch_controller.h
    src/ui/pages/quick_switch/quick_switch_controller.cpp
    src/ui/pages/home/home_controller.h
    src/ui/pages/home/home_controller.cpp
    src/ui/pages/replica_list/replica_list_page.h
    src/ui/pages/replica_list/replica_list_page.cpp
    src/ui/pages/region_preset/region_preset_controller.h
    src/ui/pages/region_preset/region_preset_controller.cpp
    src/ui/common/page_widgets.h
    src/ui/common/page_widgets.cpp
    src/ui/pages/region_preset/region_preset_page.h
    src/ui/pages/region_preset/region_preset_page.cpp
    src/ui/pages/quick_switch/quick_switch_page.h
    src/ui/pages/quick_switch/quick_switch_page.cpp
    src/ui/pages/color_rule/color_rule_page.h
    src/ui/pages/color_rule/color_rule_page.cpp
    src/ui/pages/color_rule/color_rule_dialog.h
    src/ui/pages/color_rule/color_rule_dialog.cpp
    src/ui/pages/color_rule/color_rule_controller.h
    src/ui/pages/color_rule/color_rule_controller.cpp
    src/ui/pages/home/home_page.h
    src/ui/pages/home/home_page.cpp
    src/ui/account_switch_controller.cpp
    src/ui/account_switch_controller.h
    src/ui/region_capture_controller.cpp
    src/ui/region_capture_controller.h
    src/ui/replica_binding.cpp
    src/ui/replica_binding.h
    src/ui/audio_feedback_manager.cpp
    src/ui/audio_feedback_manager.h
    src/ui/desktop_pet_page.cpp
    src/ui/desktop_pet/desktop_pet_constants.h
    src/ui/desktop_pet/avatar_painting.h
    src/ui/desktop_pet/avatar_painting.cpp
    src/ui/desktop_pet/avatar_crop_widget.h
    src/ui/desktop_pet/avatar_preview_widget.h
    src/ui/desktop_pet/floating_pet_window.h
    src/ui/desktop_pet/avatar_crop_widget.cpp
    src/ui/desktop_pet/avatar_preview_widget.cpp
    src/ui/desktop_pet/floating_pet_window.cpp
    src/ui/desktop_pet/floating_pet_messages.cpp
    src/ui/desktop_pet/floating_pet_input.cpp
    src/ui/desktop_pet/floating_pet_geometry.cpp
    src/ui/desktop_pet/desktop_pet_layout.cpp
    src/ui/desktop_pet/desktop_pet_avatar.cpp
    src/ui/desktop_pet/desktop_pet_lifecycle.cpp
    src/ui/desktop_pet/desktop_pet_messages.cpp
    src/ui/desktop_pet/desktop_pet_settings.cpp
    src/ui/desktop_pet_page.h
    src/ui/expedition_timing_page.cpp
    src/ui/expedition_timing_page.h
    src/ui/configuration_pages.h
    src/ui/main_window.cpp
    src/ui/main_window/main_window_home.cpp
    src/ui/main_window/main_window_region_presets.cpp
    src/ui/main_window/main_window_color_rules.cpp
    src/ui/main_window/main_window_page_partition.cpp
    src/ui/main_window/main_window_desktop_pet.cpp
    src/ui/main_window/main_window_quick_switch.cpp
    src/ui/main_window/main_window_configuration.cpp
    src/ui/main_window/main_window_replica_lifecycle.cpp
    src/ui/main_window/main_window_replica_restore.cpp
    src/ui/main_window/main_window_replica_sync.cpp
    src/ui/main_window/main_window_replica_bindings.cpp
    src/ui/main_window/main_window_replica_list.cpp
    src/ui/main_window/main_window_replica_stacking.cpp
    src/ui/main_window/main_window_replica_flash.cpp
    src/ui/main_window/main_window_runtime_policy.cpp
    src/ui/main_window/main_window_shutdown.cpp
    src/ui/main_window/main_window_tray.cpp
    src/ui/main_window/main_window_layout.cpp
    src/ui/main_window/main_window_region_capture.cpp
    src/ui/main_window/main_window_window_catalog.cpp
    src/ui/main_window.h
    src/ui/page_partition_page.cpp
    src/ui/page_partition_page.h
    src/ui/radial_switch_overlay.cpp
    src/ui/radial_switch_overlay.h
    src/ui/screen_region_capture_overlay.cpp
    src/ui/screen_region_capture_overlay.h
    src/ui/replica_row.cpp
    src/ui/replica_row/replica_row_constants.h
    src/ui/replica_row/scroll_passthrough_controls.h
    src/ui/replica_row/replica_row_source.cpp
    src/ui/replica_row/replica_row_region.cpp
    src/ui/replica_row/replica_row_geometry.cpp
    src/ui/replica_row/replica_row_statistics.cpp
    src/ui/replica_row/replica_row_color_logic.cpp
    src/ui/replica_row/replica_row_visibility.cpp
    src/ui/replica_row/replica_row_grouping.cpp
    src/ui/replica_row/replica_row_drag_drop.cpp
    src/ui/replica_row/replica_row_layout.cpp
    src/ui/replica_row.h
    src/ui/replica_window.cpp
    src/ui/replica_window/replica_window_menu.cpp
    src/ui/replica_window/replica_window_input.cpp
    src/ui/replica_window/replica_window_geometry.cpp
    src/ui/replica_window/replica_window_source.cpp
    src/ui/replica_window/replica_window_capture.cpp
    src/ui/replica_window/replica_window_statistics.cpp
    src/ui/replica_window/replica_window_color_logic.cpp
    src/ui/replica_window/replica_window_activation.cpp
    src/ui/replica_window/replica_window_feedback.cpp
    src/ui/replica_window/replica_window_presentation.cpp
    src/ui/replica_window.h
)
set_target_properties(eve_viola_cpp_ui PROPERTIES AUTOMOC ON)
target_compile_features(eve_viola_cpp_ui PUBLIC cxx_std_20)
target_include_directories(eve_viola_cpp_ui PUBLIC src)
target_link_libraries(eve_viola_cpp_ui PUBLIC
    eve_viola_cpp_core
    Qt6::Concurrent
    Qt6::Multimedia
    Qt6::Widgets
)
if(WIN32)
    target_link_libraries(eve_viola_cpp_ui PRIVATE user32)
endif()

add_executable(cpp_replica_window_test tests/replica_window_test.cpp)

add_executable(cpp_desktop_pet_widgets_test tests/desktop_pet_widgets_test.cpp)
set_target_properties(cpp_desktop_pet_widgets_test PROPERTIES AUTOMOC ON)
target_link_libraries(cpp_desktop_pet_widgets_test PRIVATE eve_viola_cpp_ui Qt6::Test)
add_test(NAME cpp_desktop_pet_widgets_test COMMAND cpp_desktop_pet_widgets_test)
set_tests_properties(cpp_desktop_pet_widgets_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)

set_target_properties(cpp_replica_window_test PROPERTIES AUTOMOC ON)
target_compile_features(cpp_replica_window_test PRIVATE cxx_std_20)
target_include_directories(cpp_replica_window_test PRIVATE src)
target_link_libraries(cpp_replica_window_test PRIVATE
    eve_viola_cpp_ui
    Qt6::Test
)

add_test(NAME cpp_replica_window_test COMMAND cpp_replica_window_test)
set_tests_properties(cpp_replica_window_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)

add_executable(cpp_main_window_test tests/main_window_test.cpp)
set_target_properties(cpp_main_window_test PROPERTIES AUTOMOC ON)
target_compile_features(cpp_main_window_test PRIVATE cxx_std_20)
target_include_directories(cpp_main_window_test PRIVATE src)
target_link_libraries(cpp_main_window_test PRIVATE eve_viola_cpp_ui Qt6::Test)

add_test(NAME cpp_main_window_test COMMAND cpp_main_window_test)
set_tests_properties(cpp_main_window_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)

add_executable(cpp_expedition_timing_page_test tests/expedition_timing_page_test.cpp)
set_target_properties(cpp_expedition_timing_page_test PROPERTIES AUTOMOC ON)
target_compile_features(cpp_expedition_timing_page_test PRIVATE cxx_std_20)
target_include_directories(cpp_expedition_timing_page_test PRIVATE src)
target_link_libraries(cpp_expedition_timing_page_test PRIVATE eve_viola_cpp_ui Qt6::Test)

add_test(NAME cpp_expedition_timing_page_test COMMAND cpp_expedition_timing_page_test)
set_tests_properties(cpp_expedition_timing_page_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)

add_executable(cpp_page_partition_page_test tests/page_partition_page_test.cpp)
set_target_properties(cpp_page_partition_page_test PROPERTIES AUTOMOC ON)
target_compile_features(cpp_page_partition_page_test PRIVATE cxx_std_20)
target_include_directories(cpp_page_partition_page_test PRIVATE src)
target_link_libraries(cpp_page_partition_page_test PRIVATE eve_viola_cpp_ui Qt6::Test)

add_test(NAME cpp_page_partition_page_test COMMAND cpp_page_partition_page_test)
set_tests_properties(cpp_page_partition_page_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)

add_executable(EVE-Viola WIN32
    src/app/main.cpp
    src/platform/single_instance.cpp
    src/platform/single_instance.h
    resources.rc
    src/resources/qrc_app.cpp
)
target_compile_features(EVE-Viola PRIVATE cxx_std_20)
target_link_libraries(EVE-Viola PRIVATE eve_viola_cpp_ui Qt6::Network Qt6::Widgets)

add_test(NAME cpp_app_smoke COMMAND EVE-Viola --smoke-exit-ms 250)
set_tests_properties(cpp_app_smoke PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    TIMEOUT 60
)

add_executable(cpp_single_instance_test
    tests/single_instance_test.cpp
    src/platform/single_instance.cpp
)
set_target_properties(cpp_single_instance_test PROPERTIES AUTOMOC ON)
target_compile_features(cpp_single_instance_test PRIVATE cxx_std_20)
target_include_directories(cpp_single_instance_test PRIVATE src)
target_link_libraries(cpp_single_instance_test PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Test
)

add_test(NAME cpp_single_instance_test COMMAND cpp_single_instance_test)
set_tests_properties(cpp_single_instance_test PROPERTIES
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)
