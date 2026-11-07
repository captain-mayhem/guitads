# Pinned revisions of guitads' sibling checkouts.
#
# guitads is built from three sibling repositories (tads-runner, the top-level
# CMake project; guitads; Captain's Game Engine), but only guitads is tagged
# for a release. dependencies.txt, next to this folder, records the exact
# commits of the other two that belong to this guitads commit, so a release
# tag fully determines what gets built:
#
#   - the GitHub workflows check out those commits (build.yml, release.yml)
#   - configuring checks the sibling checkouts against them
#     (guitads_check_pinned_deps(), called from ../CMakeLists.txt): a warning
#     by default, an error with -DGUITADS_REQUIRE_PINNED_DEPS=ON
#   - deps.cmake records the current commits (update) or checks them out
#     (checkout), see there
#
# dependencies.txt holds one "name=full-commit-sha" line per sibling; lines
# starting with # are comments. The names are the sibling directory names.

set(GUITADS_DEPS_FILE "${CMAKE_CURRENT_LIST_DIR}/../dependencies.txt")
set(GUITADS_DEPS_NAMES tads-runner captainsengine)

# Read dependencies.txt: sets GUITADS_PIN_<name> for each entry.
function(guitads_read_pins)
	if (NOT EXISTS "${GUITADS_DEPS_FILE}")
		message(FATAL_ERROR "${GUITADS_DEPS_FILE} is missing")
	endif()
	file(STRINGS "${GUITADS_DEPS_FILE}" lines)
	foreach (line IN LISTS lines)
		if (line MATCHES "^[ \t]*([A-Za-z0-9_.-]+)[ \t]*=[ \t]*([0-9a-fA-F]+)[ \t]*$")
			set(GUITADS_PIN_${CMAKE_MATCH_1} "${CMAKE_MATCH_2}" PARENT_SCOPE)
		elseif (NOT line MATCHES "^[ \t]*(#.*)?$")
			message(FATAL_ERROR "${GUITADS_DEPS_FILE}: cannot parse line '${line}'")
		endif()
	endforeach()
endfunction()

# Run git in a checkout: sets <out_var> to its trimmed output, and
# <out_var>_RESULT to its exit code.
function(guitads_git dir out_var)
	execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${dir}" ${ARGN}
		OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE res
		OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_STRIP_TRAILING_WHITESPACE)
	if (NOT res EQUAL 0 AND err)
		set(out "${err}")
	endif()
	set(${out_var} "${out}" PARENT_SCOPE)
	set(${out_var}_RESULT "${res}" PARENT_SCOPE)
endfunction()

# Describe a sibling checkout: sets <prefix>_HEAD (full sha, empty if <dir>
# isn't a git checkout) and <prefix>_DIRTY (the modified tracked files, as
# git status prints them; untracked files don't count).
function(guitads_describe_checkout dir prefix)
	set(${prefix}_HEAD "" PARENT_SCOPE)
	set(${prefix}_DIRTY "" PARENT_SCOPE)
	if (NOT GIT_EXECUTABLE OR NOT EXISTS "${dir}/.git")
		return()
	endif()
	guitads_git("${dir}" head rev-parse HEAD)
	if (NOT head_RESULT EQUAL 0)
		return()
	endif()
	guitads_git("${dir}" dirty status --porcelain --untracked-files=no)
	set(${prefix}_HEAD "${head}" PARENT_SCOPE)
	set(${prefix}_DIRTY "${dirty}" PARENT_SCOPE)
endfunction()

# Configure-time check: are the sibling checkouts at the pinned commits?
# <tads_runner_dir> and <cge_dir> are the actual sibling folders (an empty
# <cge_dir> skips that check, for when someone else added the engine).
function(guitads_check_pinned_deps tads_runner_dir cge_dir)
	option(GUITADS_REQUIRE_PINNED_DEPS
		"Fail the configure unless tads-runner and captainsengine are clean checkouts of the commits in guitads/dependencies.txt (release builds)"
		OFF)
	if (GUITADS_REQUIRE_PINNED_DEPS)
		set(level FATAL_ERROR)
	else()
		set(level WARNING)
	endif()

	find_package(Git QUIET)
	guitads_read_pins()
	set(GUITADS_DIR_tads-runner "${tads_runner_dir}")
	set(GUITADS_DIR_captainsengine "${cge_dir}")

	set(problems "")
	foreach (name IN LISTS GUITADS_DEPS_NAMES)
		set(dir "${GUITADS_DIR_${name}}")
		set(pin "${GUITADS_PIN_${name}}")
		if (NOT dir)
			continue()
		endif()
		if (NOT pin)
			string(APPEND problems "\n  ${name}: no entry in dependencies.txt")
			continue()
		endif()
		guitads_describe_checkout("${dir}" co)
		if (NOT co_HEAD)
			string(APPEND problems "\n  ${name}: ${dir} is not a git checkout, cannot check it against ${pin}")
			continue()
		endif()
		string(TOLOWER "${pin}" pin)
		string(LENGTH "${pin}" pin_len)
		set(head_prefix "")
		if (pin_len GREATER_EQUAL 7 AND pin_len LESS_EQUAL 40)
			string(SUBSTRING "${co_HEAD}" 0 ${pin_len} head_prefix)
		endif()
		if (NOT head_prefix STREQUAL pin)
			string(SUBSTRING "${co_HEAD}" 0 12 short_head)
			string(APPEND problems "\n  ${name}: checked out ${short_head}, pinned ${pin}")
		endif()
		if (co_DIRTY)
			string(APPEND problems "\n  ${name}: has uncommitted changes to tracked files")
		endif()
	endforeach()

	if (problems)
		message(${level}
			"The sibling checkouts don't match guitads/dependencies.txt:${problems}\n"
			"Fine while developing. Before tagging a release, commit and push them and run "
			"'cmake -P guitads/cmake/deps.cmake update'; to build exactly the pinned "
			"commits, run 'cmake -P guitads/cmake/deps.cmake checkout'.")
	endif()
endfunction()
