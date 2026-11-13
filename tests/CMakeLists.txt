# cpp-httplib (header only) serves the mock API in-process. Only its header is used.
FetchContent_Declare(httplib
  URL https://github.com/yhirose/cpp-httplib/archive/refs/tags/v0.18.3.tar.gz
  URL_HASH SHA256=a0567bcd6c3fe5cef1b329b96245119047f876b49e06cc129a36a7a8dffe173e
  SOURCE_SUBDIR do-not-add)
FetchContent_MakeAvailable(httplib)
find_package(Threads REQUIRED)

add_executable(probnode_tests mock_tests.cpp)
target_include_directories(probnode_tests SYSTEM PRIVATE ${httplib_SOURCE_DIR})
target_link_libraries(probnode_tests PRIVATE probnode::probnode Threads::Threads
  $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>)
if(WIN32)
  target_link_libraries(probnode_tests PRIVATE ws2_32 crypt32)
endif()

add_executable(probnode_live_tests live_tests.cpp)
target_link_libraries(probnode_live_tests PRIVATE probnode::probnode)

# One CTest test per case, so a run names each one.
set(PROBNODE_MOCK_TESTS
  sha256_vectors
  sha256_streaming
  datasets_are_public_and_typed
  the_key_is_sent_as_bearer
  a_keyed_call_without_a_key_sends_nothing
  errors_carry_status_and_the_api_sentence
  catalog_queries
  files_send_every_filter
  sign_batches_by_one_hundred
  download_checks_sha256_and_renames
  a_sha256_mismatch_leaves_nothing
  a_failed_fetch_is_an_error_with_its_status
  download_all_signs_once_for_many
  export_book_sends_schema_and_format
  export_book_defaults_to_book_max_parquet
  login_mints_a_key_and_uses_it
  timeseries_parquet_and_ndjson
  redirects_are_not_followed_with_the_key
  object_paths_stay_inside_dest
  config_round_trips_with_the_python_cli_file
  every_market_day_schema_is_listed)
foreach(name IN LISTS PROBNODE_MOCK_TESTS)
  add_test(NAME mock.${name} COMMAND probnode_tests ${name})
  set_tests_properties(mock.${name} PROPERTIES TIMEOUT 60)
endforeach()
add_test(NAME mock.every_case_is_registered COMMAND probnode_tests --check-count 21)

set(PROBNODE_LIVE_TESTS live_datasets live_catalog live_samples live_files_need_a_key)
foreach(name IN LISTS PROBNODE_LIVE_TESTS)
  add_test(NAME live.${name} COMMAND probnode_live_tests ${name})
  # Skipped (exit 77) unless PROBNODE_LIVE_BASE_URL is set.
  set_tests_properties(live.${name} PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 120)
endforeach()
