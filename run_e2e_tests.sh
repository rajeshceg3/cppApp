#!/bin/bash

# Exit on error
set -e

APP_NAME="./build/sms_app"
TEST_DIR="e2e_tests"
# The C++ app writes config.txt to its CWD. If sms_app is in build/, CWD is build/
CONFIG_FILE_IN_APP_CWD="config.txt" # Relative to app's CWD
APP_CWD_PATH="build" # Path to the app's expected CWD from project root
FULL_CONFIG_PATH_FOR_SCRIPT="$APP_CWD_PATH/$CONFIG_FILE_IN_APP_CWD"


passed_tests=0
failed_tests=0

# Optional: Rebuild the application
if [[ "$1" == "--rebuild" ]]; then
    echo "Rebuilding application..."
    # Assuming Makefile is in project root
    make clean && make
    shift # Remove --rebuild from arguments
fi

if [ ! -f "$APP_NAME" ]; then
    echo "ERROR: Application $APP_NAME not found. Build it first (e.g., with 'make' or './run_e2e_tests.sh --rebuild')."
    exit 1
fi

# Ensure the app's CWD exists if it's not project root, for config file placement
if [ ! -d "$APP_CWD_PATH" ]; then
    echo "ERROR: Application CWD $APP_CWD_PATH does not exist. Check build process."
    exit 1
fi

# Function to run a single test case
run_test_case() {
    local scenario_file_base="$1"
    local scenario_file_relpath="${scenario_file_base}.scenario" # Relative to TEST_DIR
    local scenario_file_abs_path="$TEST_DIR/$scenario_file_relpath"

    # Expected files are in TEST_DIR with the full base name
    local expected_out_file="$TEST_DIR/${scenario_file_base}.expected.out"
    local expected_err_file="$TEST_DIR/${scenario_file_base}.expected.err"

    # Actual files will be moved from where the app creates them
    # App creates them as <scenario_basename>.out/err in its CWD (which is TEST_DIR for this script's execution of app)
    # For clarity, let's define where the app will write them if its CWD *was* TEST_DIR
    # However, the C++ code currently derives output file paths based on the *scenario file path itself*,
    # stripping the path and original extension, then adding .out/.err.
    # So, if scenario is e2e_tests/sc01_first_run.scenario, output is e2e_tests/sc01_first_run.out
    local app_generated_out_relpath="${scenario_file_base}.out"
    local app_generated_err_relpath="${scenario_file_base}.err"
    local app_generated_out_abs_path="$TEST_DIR/$app_generated_out_relpath"
    local app_generated_err_abs_path="$TEST_DIR/$app_generated_err_relpath"

    # These are the files we'll use for diffing, after moving app_generated_*
    local actual_out_file_for_diff="$TEST_DIR/${scenario_file_base}.actual.out"
    local actual_err_file_for_diff="$TEST_DIR/${scenario_file_base}.actual.err"

    local specific_config_template="$TEST_DIR/${scenario_file_base}.config.txt"

    echo "-----------------------------------------------------"
    echo "RUNNING TEST: $scenario_file_base"
    echo "Scenario File (abs): $scenario_file_abs_path"
    echo "-----------------------------------------------------"

    # Cleanup previous actual outputs and app's config.txt from its CWD
    rm -f "$app_generated_out_abs_path" "$app_generated_err_abs_path" \
          "$actual_out_file_for_diff" "$actual_err_file_for_diff" \
          "$FULL_CONFIG_PATH_FOR_SCRIPT"

    # Setup specific config.txt if provided for the test
    # This config file is placed where the *application* expects to find it (its CWD)
    if [ -f "$specific_config_template" ]; then
        echo "Using specific config template: $specific_config_template"
        echo "Copying to: $FULL_CONFIG_PATH_FOR_SCRIPT"
        cp "$specific_config_template" "$FULL_CONFIG_PATH_FOR_SCRIPT"
    fi

    # Run the application in test mode.
    # The application executable $APP_NAME is run from the project root.
    # The scenario file path $scenario_file_abs_path is absolute or relative to project root.
    # The C++ code will create .out/.err files based on g_scenario_filepath (e.g., e2e_tests/sc01.out)
    "$APP_NAME" --test-mode "$scenario_file_abs_path" > /dev/null 2> /dev/null || true
    # `|| true` because the app might exit with failure code, which is expected in some tests. Diff will catch issues.
    # The C++ app's internal redirection is what matters. Script's redirection here is just to silence stdout/stderr of the test runner itself.

    # Move the app-generated .out and .err files to .actual.out and .actual.err for diffing
    # The C++ code creates output files like e2e_tests/sc01_first_run.out (with path)
    mv "$app_generated_out_abs_path" "$actual_out_file_for_diff" 2>/dev/null || {
        echo "WARN: App generated .out file ($app_generated_out_abs_path) not found as expected. Creating empty actual file."
        touch "$actual_out_file_for_diff"
    }
    mv "$app_generated_err_abs_path" "$actual_err_file_for_diff" 2>/dev/null || {
        echo "WARN: App generated .err file ($app_generated_err_abs_path) not found as expected. Creating empty actual file."
        touch "$actual_err_file_for_diff"
    }

    # Compare actual output with expected output
    local test_passed=true
    echo "Comparing stdout ($actual_out_file_for_diff vs $expected_out_file)..."
    if diff -u "$expected_out_file" "$actual_out_file_for_diff"; then
        echo "STDOUT matches expected."
    else
        echo "STDOUT differs for $scenario_file_base."
        test_passed=false
    fi

    echo "Comparing stderr ($actual_err_file_for_diff vs $expected_err_file)..."
    if diff -u "$expected_err_file" "$actual_err_file_for_diff"; then
        echo "STDERR matches expected."
    else
        echo "STDERR differs for $scenario_file_base."
        test_passed=false
    fi

    if $test_passed; then
        echo "TEST $scenario_file_base: PASSED"
        passed_tests=$((passed_tests + 1))
        # Optional: remove actual files on pass
        # rm "$actual_out_file_for_diff" "$actual_err_file_for_diff"
    else
        echo "TEST $scenario_file_base: FAILED"
        failed_tests=$((failed_tests + 1))
        echo "Actual stdout ($actual_out_file_for_diff) and stderr ($actual_err_file_for_diff) are kept for review."
    fi

    # Cleanup config.txt for next test, unless it's sc01 (which creates it for sc02)
    # Scenarios that use a specific template will have $FULL_CONFIG_PATH_FOR_SCRIPT cleaned at the start of their run.
    # sc01 is unique because it *creates* config.txt that sc02 *uses*.
    if [[ "$scenario_file_base" != "sc01_first_run" && ! -f "$specific_config_template" ]]; then
         rm -f "$FULL_CONFIG_PATH_FOR_SCRIPT"
    elif [[ "$scenario_file_base" == "sc01_first_run" ]]; then
        echo "INFO: Preserving config.txt created by sc01_first_run for sc02_load_config_send."
    fi


    echo ""
}

# Define test cases to run (base names without .scenario extension, relative to $TEST_DIR)
# Ensure sc01 runs first as sc02 depends on its config.txt output
run_test_case "sc01_first_run"
run_test_case "sc02_load_config_send"
run_test_case "sc04_invalid_creds_api_fail"
run_test_case "sc05a_invalid_recipient_format"
run_test_case "sc06a_incomplete_config_file" # Needs e2e_tests/sc06a_incomplete_config_file.config.txt
run_test_case "sc06b_malformed_config_file"   # Needs e2e_tests/sc06b_malformed_config_file.config.txt

# TODO: Add other test cases here once their .scenario and .expected files are created
# run_test_case "sc03_load_decline_manual"
# run_test_case "sc05b_invalid_recipient_api_fail"


echo "-----------------------------------------------------"
echo "E2E Test Summary:"
echo "Passed: $passed_tests"
echo "Failed: $failed_tests"
echo "-----------------------------------------------------"

# Do not cleanup temp_configs directory as it's not used by this version of the script

if [ "$failed_tests" -ne 0 ]; then
    exit 1
fi
exit 0
