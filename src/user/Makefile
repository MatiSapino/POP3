include Makefile.inc

SRC := $(shell find . -name "*.c")

OBJ := $(pathsubst $(SRC_DIR)/%.c, $(BIN_DIR)/%.c, $(SRC))

TARGET := $(BIN_DIR)/pop3d-user

all: $(TARGET)

$(TARGET): $(SRC)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -rf $(BIN_DIR)/pop3d-user
