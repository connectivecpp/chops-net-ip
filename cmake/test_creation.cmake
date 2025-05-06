# Copyright (c) 2024-2025 by Cliff Green
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

# add test to test suite
foreach ( test_app_name IN LISTS test_app_names )
  message ( "Creating test: run_${test_app_name}" )
  add_test ( NAME run_${test_app_name} COMMAND ${test_app_name} )
  set_tests_properties ( run_${test_app_name}
    PROPERTIES PASS_REGULAR_EXPRESSION "All tests passed"
  )
endforeach()

