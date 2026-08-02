CC = gcc
LD = ld
CPPFLAGS = -I../guest/inc -I../common
CFLAGS = -m64 -ffreestanding -fno-pic -fno-stack-protector -mno-red-zone -mgeneral-regs-only -Wall -Wextra

BUILD_DIR ?= build

A_IMAGES = $(BUILD_DIR)/A/a1_hello_world.img $(BUILD_DIR)/A/a2_echo.img
B_IMAGES = $(BUILD_DIR)/B/b1_create_write_read.img $(BUILD_DIR)/B/b2_open_flags.img \
	$(BUILD_DIR)/B/b3_shared_reader.img $(BUILD_DIR)/B/b3_shared_writer.img \
	$(BUILD_DIR)/B/b4_seek_cur_append.img $(BUILD_DIR)/B/b5_truncate_seek_end.img \
	$(BUILD_DIR)/B/b6_truncate.img $(BUILD_DIR)/B/b7_append.img \
	$(BUILD_DIR)/B/b8_seek_cur.img $(BUILD_DIR)/B/b9_seek_end.img \
	$(BUILD_DIR)/B/b10_exclusive.img $(BUILD_DIR)/B/b11_unlink.img \
	$(BUILD_DIR)/B/b12_ftruncate.img

.PHONY: all clean

all: $(A_IMAGES) $(B_IMAGES)

$(BUILD_DIR)/A $(BUILD_DIR)/B:
	mkdir -p $@

$(BUILD_DIR)/file.o: ../guest/src/file.c | $(BUILD_DIR)/B
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/A/%.o: A/%.c | $(BUILD_DIR)/A
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/B/%.o: B/%.c | $(BUILD_DIR)/B
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/A/%.img: $(BUILD_DIR)/A/%.o ../guest/guest.ld
	$(LD) -T ../guest/guest.ld $< -o $@

$(BUILD_DIR)/B/%.img: $(BUILD_DIR)/B/%.o $(BUILD_DIR)/file.o ../guest/guest.ld
	$(LD) -T ../guest/guest.ld $< $(BUILD_DIR)/file.o -o $@

clean:
	rm -rf $(BUILD_DIR)
