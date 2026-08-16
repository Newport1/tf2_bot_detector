#!/bin/sh
# Maintainer policy fence: this fork is a separate process that reads
# tf/console.log and talks to TF2 over localhost RCON only. CI fails if
# injection, overlay, hooking, or input-synthesis primitives reappear in
# maintained production source. Not a claim about VAC internals.
set -eu

cd "$(dirname "$0")/.."

pattern='ReadProcessMemory|WriteProcessMemory|VirtualAllocEx|CreateRemoteThread|SetWindowsHookEx|MinHook|PolyHook|ptrace|/proc/<pid>/mem|LD_PRELOAD|SendInput|keybd_event|TF2BD_OVERLAY_BUILD|RunProgramOverlay|-insecure'

if grep -rnE --exclude-dir=submodules --exclude-dir=other_repos --exclude-dir=build \
	--exclude='check-safety-boundary.sh' \
	"$pattern" \
	tf2_bot_detector tf2_bot_detector_common tf2_bot_detector_renderer
then
	exit 1
fi

echo "safety boundary OK"
