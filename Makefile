CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Iinclude

SRC_DIR = src
CLIENT_DIR = Redis-Client
BUILD_DIR = build

SERVER_OBJS = $(BUILD_DIR)/main.o $(BUILD_DIR)/RedisServer.o $(BUILD_DIR)/RedisCommandHandler.o
CLIENT_OBJS = $(BUILD_DIR)/cli.o

SERVER_TARGET = my_redis_server
CLIENT_TARGET = my_redis_cli

all: $(SERVER_TARGET) $(CLIENT_TARGET)

$(BUILD_DIR):
	mkdir $(BUILD_DIR) 2>nul || exit 0

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/cli.o: $(CLIENT_DIR)/cli.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(SERVER_TARGET): $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) $(SERVER_OBJS) -o $(SERVER_TARGET) -lws2_32

$(CLIENT_TARGET): $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) $(CLIENT_OBJS) -o $(CLIENT_TARGET) -lws2_32

clean:
	-rd /s /q $(BUILD_DIR) 2>nul
	-del /f /q $(SERVER_TARGET).exe $(SERVER_TARGET) $(CLIENT_TARGET).exe $(CLIENT_TARGET) 2>nul

rebuild: clean all
