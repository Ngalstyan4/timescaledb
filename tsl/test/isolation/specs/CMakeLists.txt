set(TEST_FILES
  continuous_aggs_insert.spec
  continuous_aggs_multi.spec)

set(TEST_TEMPLATES
 reorder_deadlock.spec.in
 reorder_vs_insert_other_chunk.spec.in)

set(TEST_TEMPLATES_DEBUG
  reorder_vs_insert.spec.in
  reorder_vs_select.spec.in)

if (CMAKE_BUILD_TYPE MATCHES Debug)
  list(APPEND TEST_TEMPLATES ${TEST_TEMPLATES_DEBUG})
endif(CMAKE_BUILD_TYPE MATCHES Debug)

foreach(TEMPLATE_FILE ${TEST_TEMPLATES})
  get_filename_component(TEMPLATE ${TEMPLATE_FILE} NAME_WE)
  set(TEST_FILE ${TEMPLATE}.spec)
  configure_file(${TEMPLATE_FILE} ${TEST_FILE})
  list(APPEND TEST_FILES "${TEST_FILE}")
endforeach(TEMPLATE_FILE)

file(REMOVE ${ISOLATION_TEST_SCHEDULE})

foreach(TEST_FILE ${TEST_FILES})
    string(REGEX REPLACE "(.+)\.spec" "\\1" TESTS_TO_RUN ${TEST_FILE})
    file(APPEND ${ISOLATION_TEST_SCHEDULE} "test: ${TESTS_TO_RUN}\n")
endforeach(TEST_FILE)
