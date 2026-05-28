#!/bin/bash
# Regression test runner for MiniOB
# Usage: ./run_regression.sh <test_name>  (e.g. primary-aggregation-func)
#        ./run_regression.sh all

set -e

PROJECT_DIR="/root/miniob/miniob-2023-09fe80d885b23450fdb01d7b9276ad0f0b6151c0"
OBSERVER="$PROJECT_DIR/build_debug/bin/observer"
TEST_DIR="$PROJECT_DIR/test/case/test"
RESULT_DIR="$PROJECT_DIR/test/case/result"
LOG_DIR="$PROJECT_DIR/orchestration/logs"

run_one_test() {
    local test_name="$1"
    local test_file="$TEST_DIR/${test_name}.test"
    local result_file="$RESULT_DIR/${test_name}.result"
    local output_file="$LOG_DIR/${test_name}.out"
    local filtered_file="$LOG_DIR/${test_name}.filtered"

    if [ ! -f "$test_file" ]; then
        echo "  SKIP: $test_name (no test file)"
        return 0
    fi

    echo "  Running: $test_name"

    # Run observer in CLI mode with the test file as stdin
    cd "$PROJECT_DIR"
    timeout 30 cat "$test_file" | "$OBSERVER" -P cli -f etc/observer.ini > "$output_file" 2>&1 || true

    # Filter out "miniob >" prompts and empty lines around them
    sed 's/^miniob > //g; /^miniob >$/d; /^miniob > /d' "$output_file" \
        | grep -v '^miniob' \
        | sed '/^[[:space:]]*$/N; /^\n$/d' \
        > "$filtered_file" 2>/dev/null || true

    if [ ! -f "$result_file" ]; then
        echo "  WARN: $test_name (no result file for comparison)"
        return 0
    fi

    # Compare filtered output with expected result
    if diff -w -B "$filtered_file" "$result_file" > "$LOG_DIR/${test_name}.diff" 2>&1; then
        echo "  PASS: $test_name"
        return 0
    else
        echo "  FAIL: $test_name (diff saved to $LOG_DIR/${test_name}.diff)"
        return 1
    fi
}

# Main
case "${1:-all}" in
    all)
        echo "=== Regression Test Suite ==="
        echo "Started at $(date)"
        FAIL_COUNT=0
        TESTS=(
            "primary-aggregation-func"
            "primary-date"
            "primary-drop-table"
            "primary-update"
            "primary-order-by"
        )
        for t in "${TESTS[@]}"; do
            run_one_test "$t" || FAIL_COUNT=$((FAIL_COUNT + 1))
        done
        echo ""
        echo "=== Results: $FAIL_COUNT failures ==="
        exit $FAIL_COUNT
        ;;
    *)
        run_one_test "$1"
        ;;
esac
