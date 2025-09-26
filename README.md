# Notice
This is a re-share of `gfn_to_pfn` as uploaded by [FarWrong](https://github.com/FarWrong).

# GFN to PFN Translation Module

Translate a Frame Number in a Guest/VM (GFN or Guest Frame Number) to the physical address
mapped to the GFN on the host's Physical Frame Number (PFN), with addition physical page 
inforamtion retrieval capabilities, i.e. being backed by HP or THP.

## Overview
> [!NOTE]  
> The kernel module needs to be loaded on the host, after the required kernel changes are applied.
> 
> You can later use `writer.c` and `reader.c` as described below to ensure everything is working as expected!

This kernel module provides functionality to:
1. Translate Guest **Frame Numbers** (GFN) to Host Virtual Addresses (HVA) and Host Physical Addresses (PFN)
2. Retrieve physical page information for given virtual addresses
3. Detect and report hugepage usage
4. Interface with KVM (Kernel-based Virtual Machine) for VM memory management

## Prerequisites

- Linux kernel headers
- KVM support enabled in the kernel
- Root privileges for module installation and usage
- Modified kernel with exposed `vm_list` symbol

## Required Kernel Modification

Before using this module, you must modify the host's kernel to expose the `vm_list` symbol:

1. Locate the KVM main source file:
```bash
cd /path/to/kernel/source/virt/kvm/kvm_main.c
```

2. Find the vm_list declaration:
```c
LIST_HEAD(vm_list);
```

3. Add the export symbol immediately after:
```c
LIST_HEAD(vm_list);
EXPORT_SYMBOL(vm_list);
```

> [!IMPORTANT]
> If you want to use the `reader.c` program:
> You need to disable the `CONFIG_STRICT_DEVMEM` option from your `.config` (set to `n`).
> Or change the state from `[*]` to `[ ]` if using the interactive `.config` editor.
> `reader.c` reads from an arbitrary physical address to verify the results written by the VM.
> For obvious security reasons, reading from any physical address is protected by default.
> Hence, you need to disable the protection for the program to work.

4. Rebuild and install the kernel:
```bash
make -j$(nproc)
sudo make modules_install
sudo make install
```

5. Update bootloader and reboot:
```bash
sudo update-grub
sudo reboot
```

## Installation

1. Compile the module:
```bash
make
```

2. Load the generated module:
```bash
sudo insmod gfn_to_pfn.ko
```

## Usage

> [!IMPORTANT]
> Addresses written to the proc entry need to be the guest's frame number.
> Giving an address from the VM's proccess's virtual address space does not work.
> You first need to read the process's pagemap to retrieve the frame number within the VM
> associated with that virtual address, pause the program before its termination in the VM 
> (to preserve its virtual address space and mapping to the VM's frames)
> and later write that address to the host's `gfn_to_pfn` proc entry to view the
> translated address residing on the physical memory.

### writer.c

Compile in the VM:
```bash
gcc ./writer.c -o ./writer
```

You can use `writer.c` in the VM and later check your results from the host using `reader.c`.

`writer.c` allocates a page using `mmap` and writes the default value `24`
128 bytes into the allocates page, and later translates that virtual address's 
frame number in the VM, provides the physical address within the frame in the VM,
provides the result, and puts the program in pause with a simple `getchar()`.

You can pass in another i32 value to write (default if not provided is 24 decimal):
```bash
sudo ./writer 40
```
Make sure to run with root previleges as we need to read the `pagemap` to get the GFN.

When provided the *physical address in the VM*, go ahead and write
that value to the proc entry created by the loaded module **on the host**.

### Using the kernel module on the host

The module creates a procfs entry at `/proc/gfn_to_pfn` after loaded. 

1. Write the GFN value from the VM to the proc entry. e.g.:
```bash
echo "0x1234" > /proc/gfn_to_pfn
```

or you could check multiple GFNs. e.g.:
```bash
echo "0x1111 0x2222 0x3333 0x4444" > /proc/gfn_to_pfn
```

2. Check kernel logs for the translation results:
```bash
sudo dmesg --color=always | tail
```

### Output Format

The module provides the following information in the kernel logs:
- GFN to HPA mapping
- Hugepage status (THP or regular hugepage)
- Any errors encountered during translation

Sample input:
```bash
sudo echo "0x1111 0x2222 0x3333 0x4444" > /proc/gfn_to_pfn
```

Sample output:
```bash
$ sudo dmesg --color=always | tail
[330675.275185] exact phys addr for gpa 0x1111: 0x1a5c01111
[330675.275187] page is not huge page
[330675.275188] exact phys addr for gpa 0x2222: 0x1ad725222
[330675.275190] page is not huge page
[330675.275191] exact phys addr for gpa 0x3333: 0x1ad725333
[330675.275192] page is not huge page
[330675.275193] exact phys addr for gpa 0x4444: 0x1ad725444
```

### reader.c

Compile on the host:
```bash
gcc ./reader.c -o ./reader
```

If you'd like to check a value written to a tranlsated physical address by the 
kernel module, you can pass the physical address to `reader.c` on the host
and see the value written (in decimal). e.g.:
```bash
$ sudo ./reader 0x412380
24
```

### gfn_test helper

Build the lightweight host-side client with the provided make target:
```bash
make gfn_test
```

Run it to send a single query to `/proc/gfn_to_pfn` and print the reply that the
kernel module makes available through the proc file:
```bash
sudo ./gfn_test 0x1234
Kernel reply: ok phys=0x1ad725234 kind=base gpa=0x1234 hva=0xffff9d7bc000
```

You can optionally specify a VM PID to select a particular KVM instance when
multiple VMs are running:
```bash
sudo ./gfn_test 0x1234 4242
```

The helper opens the proc entry, writes the GFN (and optional PID), then reads
the module's formatted response—handy for quick validation without tailing
kernel logs.

## Implementation Details

### Key Functions

- `get_user_page_info()`: Retrieves detailed page information for a given virtual address
- `print_gfn_to_hva()`: Performs GFN to HVA translation and prints results
- `gfn_write()`: Handles user input through the proc interface

### Memory Management
- Pages are properly acquired using `get_user_pages_remote()`
- References are released using `put_page()`
  
### Hugepage Detection

The module includes hugepage detection with two methods:
- `PageTransHuge()`: Detects Transparent Huge Pages (THP)
- `PageHuge()`: Detects regular huge pages

**Note**: The hugepage detection mechanisms should be used with caution as indicated in the source comments.

## License

This module is licensed under GPL. See the MODULE_LICENSE declaration in the source code.

## Author

Edward Guo
