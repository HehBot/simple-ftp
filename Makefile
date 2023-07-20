BIN_DIR = bin

CLIENT = $(BIN_DIR)/ftp-client
SERVER = $(BIN_DIR)/ftp-server

BINS = $(CLIENT) $(SERVER)

all: $(BINS)

clean:
	$(RM) $(BINS)

$(BIN_DIR)/%: %.c
	@mkdir -p $(dir $@)
	$(CC) -Wall -Wpedantic -Werror -g -o $@ $< -lpthread
