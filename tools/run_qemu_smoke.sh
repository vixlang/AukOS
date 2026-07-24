#!/bin/sh

set -eu

qemu=${QEMU:-qemu-system-x86_64}
e2fsck=${E2FSCK:-e2fsck}
debugfs=${DEBUGFS:-debugfs}

if [ "$#" -ne 10 ]; then
    echo "usage: $0 MODE ISO FIXTURE WORK_BASE FIRST_LOG SECOND_LOG PCAP ECHO_HELPER OVMF FINAL_WORK" >&2
    exit 2
fi

mode=$1
iso=$2
fixture=$3
work_base=$4
first_log=$5
second_log=$6
pcap=$7
echo_helper=$8
ovmf=$9
final_work=${10}
first_pcap=${pcap%.pcap}-first.pcap
echo_port=45454
echo_log=${second_log%.log}-udp-echo.log
lock_file=${second_log%/*}/udp-smoke.lock
fixture_copy=${second_log%.log}-fixture.img
guest_object=${final_work%.img}-nasm-hello.o
guest_executable=${final_work%.img}-nasm-hello
echo_pid=
qemu_pid=

exec 9>"$lock_file"
if ! flock -w 40 9; then
    echo "UDP smoke port lock timeout" >&2
    exit 1
fi

cleanup()
{
    if [ -n "$echo_pid" ]; then
        kill "$echo_pid" 2>/dev/null || true
        wait "$echo_pid" 2>/dev/null || true
    fi
    if [ -n "$qemu_pid" ] && kill -0 "$qemu_pid" 2>/dev/null; then
        kill "$qemu_pid" 2>/dev/null || true
        wait "$qemu_pid" 2>/dev/null || true
    fi
    rm -f "$fixture_copy"
}

trap cleanup EXIT INT TERM
rm -f "$first_log" "$second_log" "$pcap" "$first_pcap" "$echo_log" \
      "$fixture_copy" "$final_work" "$guest_object" "$guest_executable"
cp "$fixture" "$fixture_copy"
cp "$work_base" "$final_work"
if ! command -v "$debugfs" >/dev/null 2>&1; then
    echo "error: required host tool not found: $debugfs" >&2
    exit 1
fi
"$debugfs" -w -R "write /dev/null /.aukos-smoke" "$final_work" >/dev/null 2>&1
marker_info=$("$debugfs" -R "stat /.aukos-smoke" "$final_work" 2>/dev/null)
case "$marker_info" in
    *"Type: regular"*) ;;
    *)
        echo "failed to create smoke marker in $final_work" >&2
        exit 1
        ;;
esac
"$echo_helper" "$echo_port" 2 >"$echo_log" 2>&1 &
echo_pid=$!

attempt=0
while ! grep -F "[udp_echo] READY $echo_port" "$echo_log" >/dev/null 2>&1; do
    if ! kill -0 "$echo_pid" 2>/dev/null; then
        echo "UDP echo helper exited before ready" >&2
        exit 1
    fi
    attempt=$((attempt + 1))
    if [ "$attempt" -ge 200 ]; then
        echo "UDP echo helper ready timeout" >&2
        exit 1
    fi
    sleep 0.01
done

accel_args=
if [ -w /dev/kvm ]; then
    accel_args="-accel kvm -cpu host"
fi

start_qemu()
{
    serial_log=$1
    capture=$2
    if [ "$mode" = bios ]; then
        "$qemu" $accel_args \
            -drive "file=$iso,format=raw,if=ide,media=cdrom,readonly=on" \
            -drive "file=$fixture_copy,format=raw,if=none,id=virtio-disk" \
            -device virtio-blk-pci,drive=virtio-disk,disable-modern=on \
            -drive "file=$final_work,format=raw,if=none,id=work-disk" \
            -device virtio-blk-pci,drive=work-disk,disable-modern=on \
            -netdev user,id=net0,net=10.0.2.0/24,dhcpstart=10.0.2.15 \
            -device virtio-net-pci,netdev=net0,disable-modern=on,mac=52:54:00:12:34:56 \
            -object "filter-dump,id=netdump,netdev=net0,file=$capture" \
            -boot d -serial stdio -display none -no-reboot -no-shutdown \
            >"$serial_log" 2>&1 &
    elif [ "$mode" = uefi ]; then
        "$qemu" $accel_args \
            -drive "file=$iso,format=raw,if=ide,media=cdrom,readonly=on" \
            -drive "file=$fixture_copy,format=raw,if=none,id=virtio-disk" \
            -device virtio-blk-pci,drive=virtio-disk,disable-modern=on \
            -drive "file=$final_work,format=raw,if=none,id=work-disk" \
            -device virtio-blk-pci,drive=work-disk,disable-modern=on \
            -netdev user,id=net0,net=10.0.2.0/24,dhcpstart=10.0.2.15 \
            -device virtio-net-pci,netdev=net0,disable-modern=on,mac=52:54:00:12:34:56 \
            -object "filter-dump,id=netdump,netdev=net0,file=$capture" \
            -bios "$ovmf" -serial stdio -display none -no-reboot -no-shutdown \
            >"$serial_log" 2>&1 &
    else
        echo "unknown smoke mode: $mode" >&2
        exit 2
    fi
    qemu_pid=$!
}

wait_for_boot()
{
    serial_log=$1
    marker=$2
    required_echoes=$3
    completed=0
    guest_failed=0
    attempt=0
    while kill -0 "$qemu_pid" 2>/dev/null; do
        generated_count=$(grep -Fc "[vixc_generated_exec_test] PASS" "$serial_log" 2>/dev/null || true)
        direct_count=$(grep -Fc "[vixc_direct_driver_test] PASS" "$serial_log" 2>/dev/null || true)
        echo_count=$(grep -Fc "[udp_echo] ECHO " "$echo_log" 2>/dev/null || true)
        if [ "$generated_count" -ge 16 ] && [ "$direct_count" -ge 16 ] &&
           grep -F "$marker" "$serial_log" >/dev/null 2>&1 &&
           grep -F "[toybox_test] default command dispatch PASS" "$serial_log" >/dev/null 2>&1 &&
           [ "$echo_count" -ge "$required_echoes" ]; then
            completed=1
            kill "$qemu_pid" 2>/dev/null || true
            break
        fi
        if grep -F "[ERROR]" "$serial_log" >/dev/null 2>&1 ||
           grep -F "[nasm_test] FAIL" "$serial_log" >/dev/null 2>&1 ||
           grep -F "[persistent_work] FAIL" "$serial_log" >/dev/null 2>&1 ||
           grep -F "[toybox_test] default command dispatch FAIL" "$serial_log" >/dev/null 2>&1; then
            guest_failed=1
            kill "$qemu_pid" 2>/dev/null || true
            break
        fi
        attempt=$((attempt + 1))
        if [ "$attempt" -ge 18000 ]; then
            kill "$qemu_pid" 2>/dev/null || true
            break
        fi
        sleep 0.1
    done
    set +e
    wait "$qemu_pid"
    result=$?
    set -e
    qemu_pid=
    if [ "$guest_failed" -eq 1 ]; then
        echo "QEMU smoke guest reported failure: $serial_log" >&2
        exit 1
    fi
    if [ "$completed" -ne 1 ]; then
        if [ "$result" -ne 0 ]; then
            echo "QEMU smoke exited before completion (status $result): $serial_log" >&2
        else
            echo "QEMU smoke completion timeout: $serial_log" >&2
        fi
        exit 1
    fi
}

start_qemu "$first_log" "$first_pcap"
wait_for_boot "$first_log" "[persistent_work_first_boot] PASS" 1
sh tools/check_qemu_log.sh "$first_log" first

start_qemu "$second_log" "$pcap"
wait_for_boot "$second_log" "[persistent_work_second_boot] PASS" 2
sh tools/check_qemu_log.sh "$second_log" second

"$debugfs" -R "dump /nasm-test/hello.o $guest_object" "$final_work" >/dev/null 2>&1
"$debugfs" -R "dump /nasm-test/hello $guest_executable" "$final_work" >/dev/null 2>&1
sh tools/check_nasm_guest_artifacts.sh "$guest_object" "$guest_executable"

"$e2fsck" -fn "$final_work"
