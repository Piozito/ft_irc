NAME    = ircserv

CXX     = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98

SRCS    = main.cpp \
          network/Server.cpp \
          client/Client.cpp \
          channel/Channel.cpp

OBJS    = $(SRCS:.cpp=.o)

RESET   = \033[0m
BOLD    = \033[1m
GREEN   = \033[32m
YELLOW  = \033[33m
CYAN    = \033[36m

all: $(NAME)

$(NAME): $(OBJS)
	@$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)
	@echo "$(BOLD)$(GREEN)  ✓  $(NAME) compiled successfully$(RESET)"

%.o: %.cpp
	@echo "$(CYAN)  →  compiling $<$(RESET)"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	@rm -f $(OBJS)
	@echo "$(YELLOW)  ✗  objects removed$(RESET)"

fclean: clean
	@rm -f $(NAME)
	@echo "$(YELLOW)  ✗  $(NAME) removed$(RESET)"

re: fclean all

.PHONY: all clean fclean re
