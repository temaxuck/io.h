TEST_DIR	?= t
BUILD_DIR	?= build
CC			?= gcc

TEST_BUILD_DIR	:= $(BUILD_DIR)/$(TEST_DIR)
TEST_SRCS		:= $(wildcard $(TEST_DIR)/t_*.c)
TEST_BINS		:= $(patsubst $(TEST_DIR)/%.c, $(TEST_BUILD_DIR)/%, $(TEST_SRCS))

PHONY: tests test_build_dir clean

tests: $(TEST_BINS)
	@echo "INFO: Running all tests..."
	@for t in $(TEST_BINS); do \
		echo "INFO: Running $$t..."; \
		$$t || exit 1; \
	done

$(TEST_BUILD_DIR)/%: $(TEST_DIR)/%.c | test_build_dir
	@echo "INFO: Building test file: $< -> $@"
	@$(CC) -ggdb -Wall -Wextra -Wno-unused-value -O2 -o $@ $<

test_build_dir:
	@mkdir -p $(TEST_BUILD_DIR)

clean:
	@echo "INFO: Cleaning build directory..."
	@rm -rf $(BUILD_DIR)
