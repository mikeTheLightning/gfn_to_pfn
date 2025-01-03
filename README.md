# GFN to HVA Translation Module

A Linux kernel module that enables translation between Guest Frame Numbers (GFN) and Host Virtual Addresses (HVA), with additional physical page information retrieval capabilities.

## Overview

This kernel module provides functionality to:
1. Translate Guest Frame Numbers (GFN) to Host Virtual Addresses (HVA)
2. Retrieve physical page information for given virtual addresses
3. Detect and report hugepage usage
4. Interface with KVM (Kernel-based Virtual Machine) for VM memory management

## Prerequisites

- Linux kernel headers
- KVM support enabled in the kernel
- Root privileges for module installation and usage

## Installation

1. Compile the module:
```bash
make
```

2. Load the module:
```bash
sudo insmod gfn_to_hva.ko
```

## Usage

The module creates a procfs entry at `/proc/gfn_to_hva`. To use the module:

1. Write a GFN value to the proc entry:
```bash
echo "0x1234" > /proc/gfn_to_hva
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

