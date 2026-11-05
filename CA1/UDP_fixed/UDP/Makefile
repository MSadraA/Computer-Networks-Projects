CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -pedantic -O2

all: ping-server ping-client

ping-server: main_server.cpp PingServer.cpp UDPSocket.cpp MessageParser.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

ping-client: main_client.cpp PingClient.cpp PingStats.cpp UDPSocket.cpp MessageParser.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

tmux-env: all
	@echo "Creating Tmux environment for manual testing..."
	@tmux kill-session -t udp_test 2>/dev/null || true
	tmux new-session -d -s udp_test
	tmux split-window -h -t udp_test
	tmux send-keys -t udp_test:0.0 'clear; echo "--- Server Pane ---"' C-m
	tmux send-keys -t udp_test:0.1 'clear; echo "--- Client Pane ---"' C-m
	tmux attach-session -t udp_test

clean:
	rm -f ping-server ping-client

.PHONY: all clean tmux-env
