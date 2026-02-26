NAME    = irc_channel

SRC_DIR = src
SRC     = main.cpp $(SRC_DIR)/Channel.cpp
CC      = @c++
CFLAGS  = -std=c++98 -Wall -Wextra -Werror

OBJ_DIR = obj
# Each .cpp becomes obj/<filename>.o (flattened, no subfolders)
OBJS    = $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(notdir $(SRC)))

GREEN   = $'\033[1;32m
BLUE    = $'\033[1;34m
YELLOW  = $'\033[1;33m
RESET   = $'\033[0m

all: $(NAME)
	@echo -e "$(GREEN)==============================$(RESET)"
	@echo -e "$(GREEN)|  Compilation finished!     |$(RESET)"
	@echo -e "$(GREEN)==============================$(RESET)"

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) -Iinc/ -o $@ $^

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

# Compile any .cpp into obj/<filename>.o
$(OBJ_DIR)/%.o: %.cpp | $(OBJ_DIR)
	@echo -e "$(YELLOW)Compiling $< ...$(RESET)"
	$(CC) $(CFLAGS) -Iinc/ -c -o $@ $<
	@echo -e "$(GREEN)Done: $@$(RESET)"

# Rule for .cpp in src/ folder
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	@echo -e "$(YELLOW)Compiling $< ...$(RESET)"
	$(CC) $(CFLAGS) -Iinc/ -c -o $@ $<
	@echo -e "$(GREEN)Done: $@$(RESET)"

clean:
	@rm -f $(OBJS)
	@rm -rf $(OBJ_DIR)
	@echo -e "$(BLUE)Cleaned object files$(RESET)"

fclean: clean
	@rm -f $(NAME)
	@echo -e "$(BLUE)Removed executable $(NAME)$(RESET)"

re: fclean all

.PHONY: all clean fclean re