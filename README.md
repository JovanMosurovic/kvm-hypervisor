# KVM Hypervisor — Phases A and B

## Running examples

The commands below are examples of how to build and run the project. They are
not a required sequence, and you do not need to run every command.

Use them on a system that provides the required Linux KVM environment, such as
a native Linux installation or WSL2 with KVM support. Run them from the project
root.

This branch contains the instructions and tests for phases A and B only.

## Build and run the program

Build the guest:

```bash
make -C guest clean
make -C guest
```

Build the host:

```bash
make -C host clean
make -C host
```

Run one guest VM:

```bash
./host/build/hypervisor -m 4 -p 2 -g ./guest/build/guest.img
```

Run multiple guest VMs:

```bash
./host/build/hypervisor -m 4 -p 2 \
    -g ./guest/build/guest.img ./guest/build/guest.img
```

The default guest also demonstrates file I/O. It creates a local file under
`vm_files/`. For example, inspect the file created by VM 0 with:

```bash
cat vm_files/vm_0/guest.txt
```

## Tests

Build all A and B test images:

```bash
make -C tests clean
make -C tests
```

### A1: serial output and HLT

```bash
./host/build/hypervisor -m 2 -p 4 \
    -g tests/build/A/a1_hello_world.img
```

### A2: serial input and output

```bash
printf 'ABCDE' | ./host/build/hypervisor -m 2 -p 4 \
    -g tests/build/A/a2_echo.img
```

### B1: create, write, close, reopen and read

```bash
./host/build/hypervisor -m 2 -p 4 \
    -g tests/build/B/b1_create_write_read.img
```

### B2: open flags and read/write access

```bash
./host/build/hypervisor -m 2 -p 4 \
    -g tests/build/B/b2_open_flags.img
```

### B3: copy-on-write with a shared file

```bash
printf 'ORIGINAL' > shared.txt
./host/build/hypervisor -m 4 -p 4 \
    -g tests/build/B/b3_shared_writer.img \
       tests/build/B/b3_shared_reader.img \
    -f shared.txt
cat shared.txt
```

The original `shared.txt` should still contain `ORIGINAL` after the writer VM
finishes, because the writer receives a private copy before writing.

### B4: `SEEK_CUR` and `O_APPEND`

Build and run the additional modification test:

```bash
rm -rf vm_files
make -C tests build/B/b4_seek_cur_append.img
./host/build/hypervisor -m 2 -p 4 \
    -g tests/build/B/b4_seek_cur_append.img
```

The expected output contains:

```text
B4: content="ABXD" (expected ABXD)
```

### B5: `O_TRUNC` and `SEEK_END`

Build and run the second additional file-system test:

```bash
rm -rf vm_files
make -C tests build/B/b5_truncate_seek_end.img
./host/build/hypervisor -m 2 -p 4 \
    -g tests/build/B/b5_truncate_seek_end.img
```

The expected output contains:

```text
B5: content="ABX" (expected ABX)
```

### B6: `O_TRUNC` only

```bash
rm -rf vm_files
make -C tests build/B/b6_truncate.img
./host/build/hypervisor -m 2 -p 4 \
    -g tests/build/B/b6_truncate.img
```

Expected output:

```text
B6: size=0 (expected 0)
```

### B7: `O_APPEND` only

```bash
rm -rf vm_files
make -C tests build/B/b7_append.img
./host/build/hypervisor -m 2 -p 4 \
    -g tests/build/B/b7_append.img
```

Expected output:

```text
B7: content="ABCDEF" (expected ABCDEF)
```

### B8: `SEEK_CUR` only

`SEEK_SET` is already covered by the original B2 test, so this test checks only
the new `SEEK_CUR` behavior.

```bash
rm -rf vm_files
make -C tests build/B/b8_seek_cur.img
./host/build/hypervisor -m 2 -p 4 \
    -g tests/build/B/b8_seek_cur.img
```

Expected output:

```text
B8: content="ABCDXF" (expected ABCDXF)
```

### B9: `SEEK_END` only

```bash
rm -rf vm_files
make -C tests build/B/b9_seek_end.img
./host/build/hypervisor -m 2 -p 4 \
    -g tests/build/B/b9_seek_end.img
```

Expected output:

```text
B9: content="ABCDYF" (expected ABCDYF)
```

## Optional cleanup

The guests create local files in `vm_files/`. Remove them when a clean run is
needed:

```bash
rm -rf vm_files
```
