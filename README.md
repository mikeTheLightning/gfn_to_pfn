# GFN to PFN Translation Module

A Linux kernel module that enables translation between Guest Frame Numbers (GFN) and Host Virtual Addresses (HVA)/Physical Frame Numbers (PFN), with additional physical page information retrieval capabilities.

## Overview
NOTE- THIS IS DESIGNED TO BE USED ON THE HOST;
This kernel module provides functionality to:
1. Translate Guest Frame Numbers (GFN) to Host Virtual Addresses (HVA) and Host Physical Addresses (PFN)
2. Retrieve physical page information for given virtual addresses
3. Detect and report hugepage usage
4. Interface with KVM (Kernel-based Virtual Machine) for VM memory management

## Prerequisites

- Linux kernel headers
- KVM support enabled in the kernel
- Root privileges for module installation and usage
- Modified kernel with exposed vm_list symbol

## Required Kernel Modification

Before using this module, you must modify the kernel to expose the vm_list symbol:

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

2. Load the module:
```bash
sudo insmod gfn_to_pfn.ko
```

## Usage

The module creates a procfs entry at `/proc/gfn_to_pfn`. To use the module:

1. Write a GFN value to the proc entry:
```bash
echo "0x1234" > /proc/gfn_to_pfn
```

2. Check kernel logs for the translation results:
```bash
dmesg | tail
```

### Output Format

The module provides the following information in the kernel logs:
- GFN to HVA mapping
- Physical address of the page
- Hugepage status (THP or regular hugepage)
- Any errors encountered during translation

Example output:
```
Found VM
GFN 0x1234 maps to HVA 0x7f1234000
Page Found for VA
Physical Address:0x123456000
page is part of THP
```

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
