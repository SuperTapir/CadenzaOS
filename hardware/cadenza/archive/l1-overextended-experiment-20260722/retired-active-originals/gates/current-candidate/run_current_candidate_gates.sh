#!/bin/sh
set -eu

CADENZA_GATE_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CADENZA_REPO_DIR=$(CDPATH= cd -- "$CADENZA_GATE_DIR/../../../.." && pwd)

if [ -n "${CADENZA_GATE_PYTHON:-}" ] && [ -x "$CADENZA_GATE_PYTHON" ]; then
    exec "$CADENZA_GATE_PYTHON" "$CADENZA_GATE_DIR/run_current_candidate_gates.py" "$@"
fi

for CADENZA_PYTHON_CANDIDATE in \
    "$CADENZA_REPO_DIR/.venv/bin/python3" \
    "${HOME}/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/bin/python3"
do
    if [ -x "$CADENZA_PYTHON_CANDIDATE" ]; then
        exec "$CADENZA_PYTHON_CANDIDATE" "$CADENZA_GATE_DIR/run_current_candidate_gates.py" "$@"
    fi
done

if command -v python3.12 >/dev/null 2>&1; then
    exec "$(command -v python3.12)" "$CADENZA_GATE_DIR/run_current_candidate_gates.py" "$@"
fi

echo "No working isolated Python runtime found. Set CADENZA_GATE_PYTHON explicitly." >&2
exit 1
