#!/bin/sh
# Maintainer policy fence: this fork is a separate process that reads
# tf/console.log and talks to TF2 over localhost RCON only. CI fails if
# injection, overlay, hooking, or input-synthesis primitives reappear in
# maintained production source. Not a claim about VAC internals.
set -eu

cd "$(dirname "$0")/.."

# NOTE: /proc/<pid>/mem is a placeholder in prose, never in code -- match what
# real code looks like (/proc/self/mem, /proc/%d/mem) plus the actual syscalls.
pattern='ReadProcessMemory|WriteProcessMemory|VirtualAllocEx|CreateRemoteThread|SetWindowsHookEx|MinHook|PolyHook|ptrace|PTRACE_|process_vm_readv|process_vm_writev|/proc/[^ "]*/mem|LD_PRELOAD|SendInput|keybd_event|TF2BD_OVERLAY_BUILD|RunProgramOverlay|-insecure'

# A line tagged "safety-boundary-allow" is exempt. This exists for exactly one
# case: TF2BD must never *add* -insecure, but it does have to *detect* that the
# user already put it in their own Steam launch options, so it can avoid
# appending -secure and silently overriding them. Detection is not emission.
# The tag is deliberately greppable -- review every hit before adding one.
hits=$(grep -rnE --exclude-dir=submodules --exclude-dir=other_repos --exclude-dir=build \
	--exclude='check-safety-boundary.sh' \
	"$pattern" \
	tf2_bot_detector tf2_bot_detector_common tf2_bot_detector_renderer \
	| grep -v 'safety-boundary-allow' || true)

if [ -n "$hits" ]
then
	printf '%s\n' "$hits"
	exit 1
fi

echo "safety boundary OK"
