# Host-side build of portable Cinema OS cores (no ARM toolchain required).
# For on-camera autoexec.bin, drop DryOS stubs into platform/5D3.123 and use an ARM GCC.

CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -O2 -Iinclude -Isrc
SRC = src/csp_codec.c src/stream_engine.c
OBJ = $(SRC:.c=.o)

.PHONY: all clean test_host cinema_ui_stub

all: libcinema_csp.a

libcinema_csp.a: $(OBJ)
	ar rcs $@ $^

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

cinema_ui_stub: modules/cinema_ui/cinema_ui.c src/stream_engine.c src/csp_codec.c
	$(CC) $(CFLAGS) -DCINEMA_UI_HOST_STUB $^ -o cinema_ui_stub

clean:
	rm -f $(OBJ) libcinema_csp.a cinema_ui_stub
