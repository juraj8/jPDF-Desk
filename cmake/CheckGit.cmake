#
# Git Version and name checking
#
message(DEBUG "Git checking CM assembly v0.4.0")

option(DO_GIT_CHECK "Check git settings." OFF)

if(DO_GIT_CHECK)
  include(CMakeParseArguments)

  find_package(Git)

  if(GIT_FOUND)
    message(STATUS "git found: ${GIT_EXECUTABLE} in version ${GIT_VERSION_STRING}")
  endif(GIT_FOUND)

  execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
    RESULT_VARIABLE GIT_BRANCH_RESULT
    OUTPUT_VARIABLE GIT_BRANCH)
  execute_process(COMMAND ${GIT_EXECUTABLE} log -1 --format=%h
    RESULT_VARIABLE GIT_BRANCH_HASH_RESULT
    OUTPUT_VARIABLE GIT_BRANCH_HASH)
  execute_process(COMMAND ${GIT_EXECUTABLE} config --get user.name
    RESULT_VARIABLE GIT_NAME_RESULT
    OUTPUT_VARIABLE GIT_NAME)
  execute_process(COMMAND ${GIT_EXECUTABLE} config --get user.email
    RESULT_VARIABLE GIT_EMAIL_RESULT
    OUTPUT_VARIABLE GIT_EMAIL)

  if(NOT GIT_NAME_RESULT EQUAL 0)
    message(FATAL_ERROR "\n\tGit user name not found! Please set using:\n"
      "\t'git config user.name \"Your Name\"'")
  endif()

  if(NOT GIT_NAME_RESULT EQUAL 0)
    message(FATAL_ERROR "\n\tGit user email not found! Please set using:\n"
      "\t'git config user.email \"youremail@yourdomain.com\"'")
  endif()

  string(REGEX REPLACE "\n" "" GIT_BRANCH "${GIT_BRANCH}")
  string(REGEX REPLACE "\n" "" GIT_BRANCH_HASH "${GIT_BRANCH_HASH}")
  string(REGEX REPLACE "\n" "" GIT_NAME "${GIT_NAME}")
  string(REGEX REPLACE "\n" "" GIT_EMAIL "${GIT_EMAIL}")
endif()
