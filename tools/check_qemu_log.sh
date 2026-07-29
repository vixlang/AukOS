#!/bin/sh

set -eu

if [ "$#" -ne 2 ]; then
    echo "usage: $0 SERIAL_LOG PHASE" >&2
    exit 2
fi

log_file=$1
phase=$2

require_line()
{
    if ! grep -F "$1" "$log_file" >/dev/null; then
        echo "missing QEMU smoke log: $1" >&2
        exit 1
    fi
}

require_following_line()
{
    if ! COMMAND="$1" EXPECTED="$2" awk '
        {
            line = $0
            sub(/\r$/, "", line)
            if (waiting) {
                if (line == ENVIRON["EXPECTED"]) success = 1
                exit
            }
            if (line == ENVIRON["COMMAND"]) waiting = 1
        }
        END { if (!success) exit 1 }
    ' "$log_file"; then
        echo "QEMU smoke output mismatch after: $1" >&2
        exit 1
    fi
}

require_following_lines()
{
    if ! COMMAND="$1" EXPECTED_FIRST="$2" EXPECTED_SECOND="$3" awk '
        {
            line = $0
            sub(/\r$/, "", line)
            if (state == 1) {
                if (line != ENVIRON["EXPECTED_FIRST"]) exit 1
                state = 2
                next
            }
            if (state == 2) {
                if (line == ENVIRON["EXPECTED_SECOND"]) success = 1
                exit
            }
            if (line == ENVIRON["COMMAND"]) state = 1
        }
        END { if (!success) exit 1 }
    ' "$log_file"; then
        echo "QEMU smoke output mismatch after: $1" >&2
        exit 1
    fi
}

require_command_output_contains()
{
    if ! COMMAND="$1" EXPECTED="$2" awk '
        {
            line = $0
            sub(/\r$/, "", line)
            if (waiting) {
                if (index(line, ENVIRON["EXPECTED"])) success = 1
                if (index(line, "[root@aukos") == 1) exit
            }
            if (line == ENVIRON["COMMAND"]) waiting = 1
        }
        END { if (!success) exit 1 }
    ' "$log_file"; then
        echo "QEMU smoke output missing '$2' after: $1" >&2
        exit 1
    fi
}

require_line "pci: found virtio-blk 1af4:1001 io=0x"
require_line "console: high-resolution framebuffer using Terminus 8x20 font"
require_line "virtio-blk: virtio0 fixture self-test passed"
require_line "virtio-blk: virtio1 work device initialized with FLUSH"
require_line "ext4: mounted /ext4 read-only from virtio0"
require_line "ext4: mounted /work read-write from virtio1 with flush"
require_line "pci: found virtio-net 1af4:1000 io=0x"
require_line "virtio-net: MAC 52:54:00:12:34:56"
require_line "virtio-net: receiveq/transmitq initialized (legacy split, polling)"
require_line "net: gateway ARP entry learned"
require_line "net: ICMP echo reply received from 10.0.2.2"
require_line "net: gateway ARP/IPv4/ICMP self-test passed"
require_line "[signal_test] PASS"
require_line "[dev_memory_test] PASS"
require_line "[pipe_test] PASS"
require_line "[process_env_test] PASS"
require_line "[process_stack_test] PASS"
require_line "[generated_exec_test] PASS"
require_line "[file_api_test] PASS"
if [ "$phase" = first ]; then
    require_line "[persistent_work_first_boot] PASS"
    require_line "[persistent_vix_exec_first_boot] PASS"
    require_line "[nasm_native_first_boot] PASS"
elif [ "$phase" = second ]; then
    require_line "[persistent_work_second_boot] PASS"
    require_line "[persistent_vix_exec_second_boot] PASS"
    require_line "[nasm_native_second_boot] PASS"
else
    echo "unknown persistence phase: $phase" >&2
    exit 2
fi
require_line "[vix_hello] PASS"
require_line "[vix_runtime_test] PASS"
require_line "[vixc_frontend_test] PASS"
require_line "[vixc_object_test] PASS"
require_line "[vixc_generated_exec_test] PASS"
require_line "[vixc_direct_driver_test] PASS"
require_line "[vixc_cli_test] PASS"
require_line "[vixc_temp_cleanup_test] PASS"
require_line "[vixc_output_preservation_test] PASS"
require_line "[nasm_generated_exec_test] PASS"
require_line "[nasm_cli_test] PASS"
require_line "[nasm_output_preservation_test] PASS"
require_line "[nasm_temp_cleanup_test] PASS"
require_line "[nasm_reuse_test] PASS 8/8"
require_line "[touch_test] PASS"
require_line "[ls_color_test] PASS"
require_line "[ed_visual_test] PASS"
require_line "[ed_visual_driver_test] PASS"
require_line "[ed_vix_test] PASS"
require_line "[ed_test] PASS"
require_line "[udp_test] PASS"
require_line "[shell_test] pipeline PASS"
require_line "[shell_test] redirection PASS"
require_line "[shell_test] cd PASS"
require_line "Hello from AukOS ext4!"
require_line "[root@aukos /]$ toybox"
require_line "[root@aukos /]$ echo AukOS ToyBox"
require_line "[root@aukos /]$ echo \"quoted argument\""
require_line "[root@aukos /]$ pwd"
require_line "[root@aukos /]$ cd /ext4"
require_line "[root@aukos /ext4]$ cd -"
require_line "[root@aukos /]$ cd"
require_line "[root@aukos /]$ cd /bin"
require_line "[root@aukos /bin]$ ls"
require_line "[root@aukos /bin]$ cd -"
require_line "[root@aukos /]$ ls /ext4"
require_line "[root@aukos /]$ cat /ext4/hello.txt"
require_line "[root@aukos /]$ uname -a"
require_line "[root@aukos /]$ basename /ext4/hello.txt"
require_line "[root@aukos /]$ dirname /ext4/hello.txt"
require_line "[root@aukos /]$ head -n 1 /ext4/hello.txt"
require_line "[root@aukos /]$ wc -c /ext4/hello.txt"
require_line "[root@aukos /]$ printf '%s\n' AukOS"
require_line "[root@aukos /]$ printf 'pipeline-ok\n' | wc -c"
require_line "[root@aukos /]$ echo first > /work/output"
require_line "[root@aukos /]$ echo second >> /work/output"
require_line "[root@aukos /]$ cat < /work/output"
require_line "[toybox_test] true/false exit status PASS"
require_line "[toybox_test] default command dispatch PASS"

require_following_line "[root@aukos /]$ toybox" "basename cat dirname echo false head ls printf pwd true uname wc "
require_following_line "[root@aukos /]$ echo AukOS ToyBox" "AukOS ToyBox"
require_following_line "[root@aukos /]$ echo \"quoted argument\"" "quoted argument"
require_following_line "[root@aukos /]$ pwd" "/"
require_following_lines "[root@aukos /]$ cd /ext4" "[root@aukos /ext4]$ pwd" "/ext4"
require_following_lines "[root@aukos /ext4]$ cd -" "[root@aukos /]$ pwd" "/"
require_following_lines "[root@aukos /]$ cd" "[root@aukos /]$ pwd" "/"
require_command_output_contains "[root@aukos /bin]$ ls" "toybox"
require_following_lines "[root@aukos /bin]$ cd -" "[root@aukos /]$ pwd" "/"
require_command_output_contains "[root@aukos /]$ ls /ext4" "hello.txt"
escape=$(printf '\033')
require_line "$escape[1;34m/bin$escape[0m"
require_line "$escape[1;37m/ext4/hello.txt$escape[0m"
require_line "$escape[1;32m/bin/toybox$escape[0m"
require_following_line "[root@aukos /]$ cat /ext4/hello.txt" "Hello from AukOS ext4!"
require_following_line "[root@aukos /]$ uname -a" "AukOS aukos 0.0.1 toybox-port x86_64 Toybox"
require_following_line "[root@aukos /]$ basename /ext4/hello.txt" "hello.txt"
require_following_line "[root@aukos /]$ dirname /ext4/hello.txt" "/ext4"
require_following_line "[root@aukos /]$ head -n 1 /ext4/hello.txt" "Hello from AukOS ext4!"
require_following_line "[root@aukos /]$ wc -c /ext4/hello.txt" "23 /ext4/hello.txt"
require_following_line "[root@aukos /]$ wc -c /work/touched-a" "0 /work/touched-a"
require_following_lines "[root@aukos /]$ cat /work/ed-ops" "zero" "two"
require_following_lines "[root@aukos /]$ cat /work/visual-smoke" "bc" "def"
require_line "? buffer modified"
require_following_line "[root@aukos /]$ printf '%s\n' AukOS" "AukOS"
require_following_line "[root@aukos /]$ printf 'pipeline-ok\n' | wc -c" "12"
require_following_lines "[root@aukos /]$ cat < /work/output" "first" "second"

for forbidden in "[ERROR]" "execve failed" "waitpid failed" "fork failed" \
		"unknown command" "parse error" "usage:" "[toybox_test] default command dispatch FAIL" \
		"[toybox_test] true/false exit status FAIL" "[udp_test] FAIL" \
		"[dev_memory_test] FAIL" "dev_memory_test timeout" \
		"[pipe_test] FAIL" "[process_env_test] FAIL" \
		"[process_stack_test] FAIL" \
		"[generated_exec_test] FAIL" "[file_api_test] FAIL" \
		"[vix_hello] FAIL" \
		"[vix_runtime_test] FAIL" \
		"[vixc_test] FAIL" \
		"[nasm_test] FAIL" "[persistent_work] FAIL" \
		"[touch_test] FAIL" "[ls_color_test] FAIL" "[ed_visual_test] FAIL" \
		"[ed_visual_driver_test] FAIL" "[ed_test] FAIL" \
		"[shell_test] pipeline FAIL" "[shell_test] redirection FAIL" \
		"[shell_test] cd FAIL" \
		"deadlock" \
		"UDP echo helper ready timeout" "UDP echo helper did not complete"; do
    if grep -F "$forbidden" "$log_file" >/dev/null; then
        echo "QEMU smoke log contains forbidden text: $forbidden" >&2
        exit 1
    fi
done

generated_count=$(grep -Fc "[vixc_generated_exec_test] PASS" "$log_file")
if [ "$generated_count" -lt 16 ]; then
    echo "QEMU smoke log has only $generated_count generated exec passes" >&2
    exit 1
fi

direct_count=$(grep -Fc "[vixc_direct_driver_test] PASS" "$log_file")
if [ "$direct_count" -lt 16 ]; then
    echo "QEMU smoke log has only $direct_count direct driver exec passes" >&2
    exit 1
fi

nasm_exec_count=$(grep -Fc "[nasm_generated_exec_test] PASS" "$log_file")
if [ "$nasm_exec_count" -lt 2 ]; then
    echo "QEMU smoke log has only $nasm_exec_count NASM-generated exec passes" >&2
    exit 1
fi

echo "QEMU smoke log passed: $log_file"
