CC = gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude
LDFLAGS = -lconfig -lnetsnmp -lmysqlclient
TARGET = build/async_collector
SRCS = src/main.c src/config_loader.c src/async_collector.c src/db.c

all: $(TARGET)

$(TARGET): $(SRCS) include/collector.h
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS)

clean:
	rm -f $(TARGET)
