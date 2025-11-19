CXX = g++
CXXFLAGS = -std=c++11 -pthread -Wall
TARGET_SERVER = server
TARGET_CLIENT = client

all: $(TARGET_SERVER) $(TARGET_CLIENT)

$(TARGET_SERVER): server.cpp types.h
	$(CXX) $(CXXFLAGS) server.cpp -o $(TARGET_SERVER)

$(TARGET_CLIENT): client.cpp types.h
	$(CXX) $(CXXFLAGS) client.cpp -o $(TARGET_CLIENT)

clean:
	rm -f $(TARGET_SERVER) $(TARGET_CLIENT)

run-server: $(TARGET_SERVER)
	./$(TARGET_SERVER)

run-client: $(TARGET_CLIENT)
	./$(TARGET_CLIENT)

.PHONY: all clean run-server run-client
