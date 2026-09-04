#
# Copyright (C) 2020 Alex Mayfield.
#

function(git_rev_count _refspecvar _countvar)
  if(NOT GIT_FOUND)
    find_package(Git QUIET)
  endif()

  execute_process(COMMAND
    "${GIT_EXECUTABLE}" rev-list ${_refspecvar} --count
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
		RESULT_VARIABLE res
		OUTPUT_VARIABLE out
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  set(${_countvar} "${out}" PARENT_SCOPE)
endfunction()
