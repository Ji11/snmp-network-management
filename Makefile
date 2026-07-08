CC = gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude
LDFLAGS = -lconfig -lnetsnmp -lmysqlclient
TARGET = build/snmp_collector
SRCS = src/main.c src/config_loader.c src/snmp_collector.c src/db.c

all: $(TARGET)

$(TARGET): $(SRCS) include/collector.h
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS)

clean:
	rm -f $(TARGET)
