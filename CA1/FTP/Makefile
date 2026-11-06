CXX ?= g++
CXXFLAGS ?= -std=c++11 -Wall -Wextra -Wpedantic -O2
LDFLAGS ?=
LDLIBS ?= -pthread
FTP_PORT ?= 9000
FTP_HOST ?= 127.0.0.1
FTP_ROOT ?= ./server-files

COMMON_SOURCES = TCPSocket.cpp FTPParser.cpp
SERVER_SOURCES = main_server.cpp FTPServer.cpp $(COMMON_SOURCES)
CLIENT_SOURCES = main_client.cpp FTPClient.cpp $(COMMON_SOURCES)

.PHONY: all clean

all: ftp-server ftp-client
	@command -v tmux >/dev/null 2>&1 || { echo "Error: tmux is required. Install it and run make again."; exit 1; }
	@mkdir -p "$(FTP_ROOT)"
	@tmux kill-session -t ftp_test 2>/dev/null || true
	@tmux new-session -d -s ftp_test -c "$(CURDIR)" "./ftp-server --port $(FTP_PORT) --root $(FTP_ROOT); exec bash"
	@tmux split-window -h -t ftp_test:0 -c "$(CURDIR)" "sleep 0.3; ./ftp-client --host $(FTP_HOST) --port $(FTP_PORT); exec bash"
	@tmux select-layout -t ftp_test:0 even-horizontal
	@tmux select-pane -t ftp_test:0.1
	@tmux attach-session -t ftp_test

ftp-server: $(SERVER_SOURCES) FTPServer.hpp FTPParser.hpp TCPSocket.hpp protocol.hpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(SERVER_SOURCES) $(LDLIBS)

ftp-client: $(CLIENT_SOURCES) FTPClient.hpp FTPParser.hpp TCPSocket.hpp protocol.hpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(CLIENT_SOURCES) $(LDLIBS)

clean:
	rm -f ftp-server ftp-client
