#!/bin/sh
# Local replacement for the GitHub Actions gates.
#
# The workflows in .github/workflows/ are workflow_dispatch-only: push-triggered
# builds on a private repo bill real money per commit (Windows runners at 2x), and
# this fork is not releasing yet. Run this instead, before you push.
#
#   scripts/ci-local.sh            configure if needed, build, test
#   scripts/ci-local.sh --clean    wipe the build dir first
#
# Exit code is the number of failed stages, so `&&` chaining works.
#
# ponytail: Linux only. The Windows half of CI has no local equivalent here --
# that gap is real and is not pretended away; dispatch the workflow manually if
# you need it.

set -u

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT" || exit 1

BUILD_DIR=${BUILD_DIR:-build}
FAILED=0

# cmake/ninja were pip-installed into ~/.local/bin on this machine.
PATH="$HOME/.local/bin:$PATH"
export PATH

stage() {
	printf '\n=== %s\n' "$1"
}

fail() {
	printf '!!! FAILED: %s (exit %s)\n' "$1" "$2"
	FAILED=$((FAILED + 1))
}

if [ "${1:-}" = "--clean" ]; then
	stage "clean"
	rm -rf "$BUILD_DIR"
fi

stage "safety boundary"
sh scripts/check-safety-boundary.sh
rc=$?
[ $rc -eq 0 ] || fail "safety boundary" $rc

stage "configure"
if [ -f "$BUILD_DIR/build.ninja" ]; then
	echo "(already configured; delete $BUILD_DIR or pass --clean to redo)"
else
	cmake -S . -B "$BUILD_DIR" -G Ninja \
		-DCMAKE_BUILD_TYPE=Release \
		-DTF2BD_ENABLE_TESTS=ON \
		-DCMAKE_TOOLCHAIN_FILE=submodules/vcpkg/scripts/buildsystems/vcpkg.cmake
	rc=$?
	[ $rc -eq 0 ] || { fail "configure" $rc; echo "FAILED STAGES: $FAILED"; exit $FAILED; }
fi

stage "build"
cmake --build "$BUILD_DIR"
rc=$?
[ $rc -eq 0 ] || { fail "build" $rc; echo "FAILED STAGES: $FAILED"; exit $FAILED; }

stage "tests"
"./$BUILD_DIR/tf2_bot_detector/tf2_bot_detector_cli" --run-tests
rc=$?
[ $rc -eq 0 ] || fail "tests" $rc

# Hidden tests that read the real Steam install. They SKIP rather than fail when
# Steam or TF2 is absent -- but Catch2 treats "every test skipped" as "no tests ran"
# and exits 4, so this stage was counted as FAILED on any machine without TF2
# installed, which is most of them. --allow-running-no-tests makes the all-skipped
# case exit 0 while a genuine failure still exits non-zero.
stage "tests [realsteam]"
"./$BUILD_DIR/tf2_bot_detector/tf2_bot_detector_cli" --run-tests "[realsteam]" --allow-running-no-tests
rc=$?
[ $rc -eq 0 ] || fail "tests [realsteam]" $rc

printf '\n=== FAILED STAGES: %s\n' "$FAILED"
exit $FAILED
