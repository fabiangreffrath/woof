#
# Copyright (C) 2021 Alex Mayfield.
#

function(git_branch _refspecvar _countvar)
  if(NOT GIT_FOUND)
    find_package(Git QUIET)
  endif()

  execute_process(COMMAND
    "${GIT_EXECUTABLE}" rev-parse --abbrev-ref ${_refspecvar}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
		RESULT_VARIABLE res
		OUTPUT_VARIABLE out
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  set(${_countvar} "${out}" PARENT_SCOPE)
endfunction()
