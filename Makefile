INCLUDE_PATHES = -I.
LD_LIBS = -llua
CFLAGS = -std=c11 -Wall -O3

BUILD_DIR = build
STATIC_LIB_EXT  =
DYNAMIC_LIB_EXT  =
dectedOS = $(OS)
ifneq ($(dectedOS), Windows_NT)
dectedOS = $(shell uname -s)
endif
ifeq ($(dectedOS), Windows_NT)
DYNAMIC_LIB_EXT = dll
STATIC_LIB_EXT = lib
else ifeq ($(dectedOS), Darwin)
DYNAMIC_LIB_EXT = dylib
STATIC_LIB_EXT = a
else
DYNAMIC_LIB_EXT = so
STATIC_LIB_EXT = a
endif

STATIC_LIB_NAME = $(BUILD_DIR)/libpblua.$(STATIC_LIB_EXT)
DYNAMIC_LIB_NAME := $(BUILD_DIR)/libpblua.$(DYNAMIC_LIB_EXT)

SOURCE_LUA_FILES = $(wildcard lua/*.lua)
SOURCE_LUA_FILES_GEN = lua/luafile_gen.h
SOURCE_FILES = $(wildcard lua/*.c) $(wildcard pb/*.c)
SOURCE_OBJS = $(patsubst %.c, $(BUILD_DIR)/%.o, $(SOURCE_FILES))

TEST_FILES = $(wildcard test/*.c)
TEST_OBJS = $(patsubst %.c, $(BUILD_DIR)/%.o, $(TEST_FILES))
TEST_BIN = $(BUILD_DIR)/test/test

PROTO_FILES = $(wildcard test/*.proto)

.PHONY: build install clean test test_go

#=================================================================
#                        BUILD
build: $(STATIC_LIB_NAME) $(DYNAMIC_LIB_NAME)

$(STATIC_LIB_NAME): $(SOURCE_OBJS)
	$(AR) rc $@ $^

$(DYNAMIC_LIB_NAME): $(SOURCE_OBJS)
	$(CC) -fPIC -shared $^ -o $@ $(LD_LIBS)

$(SOURCE_LUA_FILES_GEN): $(SOURCE_LUA_FILES)
	@echo > $@
	@for f in $^; do \
		xxd -i $$f >> $@; \
	done

$(BUILD_DIR)/%.o: %.c $(SOURCE_LUA_FILES_GEN)
	@mkdir -p $(shell dirname $@)
	$(CC) $(INCLUDE_PATHES) $(CFLAGS) -c -o $@ $<

install:
	cp build/libpblua.* /usr/local/lib

clean:
	$(RM) -r $(BUILD_DIR)


#==================================================================
#                        TEST
test: $(TEST_BIN) $(BUILD_DIR)/testout/proto.pb
	$<

$(BUILD_DIR)/testout/proto.pb: $(PROTO_FILES)
	@mkdir -p $(BUILD_DIR)/testout
	protoc -o $@ $^

test_go: test/msg.pb.go
	go test test/*.go -v

test/msg.pb.go: $(PROTO_FILES)
	protoc -I.:$(GOPATH)/src --gogofaster_out=Mgoogle/protobuf/any.proto=github.com/gogo/protobuf/types:. $^

$(TEST_BIN): $(TEST_OBJS) $(SOURCE_OBJS)
	$(CC) -o $@ $^ $(LD_LIBS)
