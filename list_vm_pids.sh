#!/usr/bin/env bash
# List all libvirt VMs and their QEMU PIDs

# Get VM names (both running and defined)
vms=$(virsh list --all --name)

printf "%-30s %-10s\n" "VM Name" "QEMU PID"
printf "%-30s %-10s\n" "-------" "--------"

for vm in $vms; do
    pid_file="/run/libvirt/qemu/${vm}.pid"
    if [[ -f "$pid_file" ]]; then
        pid=$(cat "$pid_file")
        printf "%-30s %-10s\n" "$vm" "$pid"
    else
        printf "%-30s %-10s\n" "$vm" "N/A"
    fi
done

