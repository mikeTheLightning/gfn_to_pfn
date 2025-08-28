#!/bin/bash

# Remote hosts
HOST=akavish-i9-10900k
VM_GUEST=phys

# Directory name for deployment
REMOTE_DIR="~/gfn_to_pfn"

# Build & run commands
COMPILE_RUN_COMMANDS_HOST=$(cat <<EOF
cd $REMOTE_DIR
gcc -o host_gfn_to_pfn_server tests/host_gfn_to_pfn_server.c
sudo bash -c './host_gfn_to_pfn_server'
EOF
)

# Build & run commands
COMPILE_RUN_COMMANDS_VM_GUEST=$(cat <<EOF
cd $REMOTE_DIR
gcc -o guest_mmap_gpa tests/guest_mmap_gpa.c
sudo ./guest_mmap_gpa 192.168.122.1
EOF
)

# Function to copy and run on a host
deploy_and_run_host() {
    local MACHINE=$1

    echo "Copying files to $MACHINE..."
    scp -r . "$MACHINE:$REMOTE_DIR"

    echo "Running compile and execution on $MACHINE..."
    ssh -t "$MACHINE" "$COMPILE_RUN_COMMANDS_HOST"
}


# Function to copy and run on a host
deploy_and_run_vm_guest() {
    local MACHINE=$1

    echo "Copying files to $MACHINE..."
    scp -r . "$MACHINE:$REMOTE_DIR"

    echo "Running compile and execution on $MACHINE..."
    ssh -t "$MACHINE" "$COMPILE_RUN_COMMANDS_VM_GUEST"
}

# Deploy to both hosts
deploy_and_run_host "$HOST"
#deploy_and_run_vm_guest "$VM_GUEST"
