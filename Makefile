# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -O2

# Directories
SRC_DIR = cmd
BIN_DIR = bin
INSTALL_DIR ?= $(HOME)/.local/bin

# Shell config files
BASHRC := $(HOME)/.bashrc
ZSHRC := $(HOME)/.zshrc
SHELL_RC := $(if $(wildcard $(ZSHRC)),$(ZSHRC),$(BASHRC))

# Source files and target executables
SRCS = $(wildcard $(SRC_DIR)/*.c)
EXES = $(patsubst $(SRC_DIR)/%.c,$(BIN_DIR)/%,$(SRCS))
BINS = $(notdir $(EXES))

.PHONY: all clean install uninstall

all: $(BIN_DIR) $(EXES)

$(BIN_DIR)/%: $(SRC_DIR)/%.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $<

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -rf $(BIN_DIR)

install: all
	@echo "Creating installation directory: $(INSTALL_DIR)"
	@mkdir -p "$(INSTALL_DIR)"
	@echo "Installing executables..."
	@for bin in $(BINS); do \
		echo " - Installing $$bin"; \
		install -m 0755 "$(BIN_DIR)/$$bin" "$(INSTALL_DIR)/$$bin"; \
	done

	@echo "Updating shell configuration..."
	@if [ -f "$(SHELL_RC)" ]; then \
		# Remove any existing ls alias \
		sed -i '/alias ls=/d' "$(SHELL_RC)"; \
		\
		# Add PATH if not already present \
		if ! grep -q 'export PATH="\$$HOME/.local/bin:\$$PATH"' "$(SHELL_RC)"; then \
			echo '\n# Added by Plan9-Util-Port\nexport PATH="$$HOME/.local/bin:$$PATH"' >> "$(SHELL_RC)"; \
			echo "Added PATH to $(SHELL_RC)"; \
		else \
			echo "PATH already configured in $(SHELL_RC)"; \
		fi; \
		\
		# For zsh, also update zshrc \
		if [ -f "$(ZSHRC)" ]; then \
			sed -i '/alias ls=/d' "$(ZSHRC)"; \
		fi; \
	else \
		echo "Creating new $(SHELL_RC)"; \
		echo '\n# Added by Plan9-Util-Port\nexport PATH="$$HOME/.local/bin:$$PATH"' > "$(SHELL_RC)"; \
	fi

	@echo "Refreshing shell environment..."
	@(unalias ls 2>/dev/null || true)
	@hash -r 2>/dev/null || true

	@echo "\nInstallation complete."
	@echo "To use the commands, either:"
	@echo "1. Run: source $(SHELL_RC)"
	@echo "2. Start a new terminal session"

uninstall:
	@echo "Removing installed executables..."
	@for bin in $(BINS); do \
		echo " - Removing $$bin"; \
		rm -f "$(INSTALL_DIR)/$$bin"; \
	done
	@echo "Uninstallation complete."

test:
	@echo "Running basic tests..."
	@for bin in $(BINS); do \
		if [ -x "$(BIN_DIR)/$$bin" ]; then \
			echo -n "Testing $$bin: "; \
			$(BIN_DIR)/$$bin --help >/dev/null 2>&1 || $(BIN_DIR)/$$bin -h >/dev/null 2>&1 || $(BIN_DIR)/$$bin >/dev/null 2>&1; \
			if [ $$? -eq 0 ]; then echo "OK"; else echo "FAILED"; fi; \
		fi \
	done