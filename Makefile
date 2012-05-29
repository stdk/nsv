CC              := /usr/local/opt/crosstool/arm-linux/gcc-3.3.4-glibc-2.3.2/arm-linux/bin/g++
STRIP           := arm-linux-strip
SRC_DIR         := src
TEST_DIR        := test
INCLUDE_DIR     := include
OBJ_DIR         := obj

CFLAGS          := -O3 -Wall -I $(INCLUDE_DIR) -fno-exceptions -finline-functions
LFLAGS          := -levent -lsqlite3

MODULES         := platform.sys platform log can timed_event task net \
                   can_device device_container can_protocol \
                   storage get_event_impl update_firmware_impl \
		   stoplist_impl command http can_handler adbk
OBJS            := $(foreach module,$(MODULES),$(OBJ_DIR)/$(module).o)

TESTS           := device_container

all: nsv

install: nsv
	scp nsv root@10.0.2.221:/mnt/cf/nsv

tests: $(foreach test,$(TESTS),$(test).test)

%.test: $(TEST_DIR)/%.cpp $(OBJS)
	$(CC) $(CFLAGS) $(LFLAGS) $^ -o $@

nsv: $(OBJS) $(OBJ_DIR)/nsv.o
	$(CC) $(LFLAGS) $^ -o $@
	$(STRIP) $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) -O2 -I $(INCLUDE_DIR) -c $^ -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CC) $(CFLAGS) -c $^ -o $@

clean:
	rm -rf $(OBJ_DIR)/*.o *.cpp~ *.h~ nsv *.test
