CC = gcc
CFLAGS = -Wall -Wextra -O2
SRC_DIR = cmd
BIN_DIR = bin
INSTALL_DIR = /usr/local/bin

SRCS = $(wildcard $(SRC_DIR)/*.c)
EXES = $(patsubst $(SRC_DIR)/%.c,$(BIN_DIR)/%,$(SRCS))
BINS = $(patsubst $(BIN_DIR)/%,%,$(EXES))  # just executable names

all: $(BIN_DIR) $(EXES)

$(BIN_DIR)/%: $(SRC_DIR)/%.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $<

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

.PHONY: all clean install uninstall

clean:
	rm -rf $(BIN_DIR)

install: all
	@echo "Installing executables to $(INSTALL_DIR)... (requires sudo)"
	@for bin in $(BINS); do \
		echo "Installing $$bin"; \
		sudo cp $(BIN_DIR)/$$bin $(INSTALL_DIR)/$$bin; \
		sudo chmod +x $(INSTALL_DIR)/$$bin; \
	done
	@echo "Installation complete."

uninstall:
	@echo "Removing installed executables from $(INSTALL_DIR)... (requires sudo)"
	@for bin in $(BINS); do \
		echo "Removing $$bin"; \
		sudo rm -f $(INSTALL_DIR)/$$bin; \
	done
	@echo "Uninstallation complete."