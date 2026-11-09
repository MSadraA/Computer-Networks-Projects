CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -pthread

PART2_BINS = chat-server chat-client

all: $(PART2_BINS)

chat-server: chat-server.cpp chat_server.hpp chat_protocol.hpp
	$(CXX) $(CXXFLAGS) -o chat-server chat-server.cpp

chat-client: chat-client.cpp chat_client.hpp chat_protocol.hpp
	$(CXX) $(CXXFLAGS) -o chat-client chat-client.cpp

clean:
	rm -f chat-server chat-client ping-server ping-client ftp-server ftp-client

.PHONY: all clean