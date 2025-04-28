CC = g++ -std=c++17
CFLAGS = -Wall

SRCDIR = src
OBJDIR = obj

TARGET_SERVER = tftp-server
TARGET_CLIENT = tftp-client

all: $(TARGET_SERVER) $(TARGET_CLIENT)

$(TARGET_SERVER): $(SRCDIR)/$(TARGET_SERVER).cpp $(OBJDIR)/tftp-communication.o $(OBJDIR)/tftp-packet-structures.o
	$(CC) $(CFLAGS) $^ -o $@

$(TARGET_CLIENT): $(SRCDIR)/$(TARGET_CLIENT).cpp $(OBJDIR)/tftp-communication.o $(OBJDIR)/tftp-packet-structures.o
	$(CC) $(CFLAGS) $^ -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CC) $(CFLAGS) -c -o $@ $<


clean_s:
	rm $(TARGET_SERVER) 

clean_c:
	rm $(TARGET_CLIENT) 

clean:
	rm $(TARGET_SERVER) $(TARGET_CLIENT) $(OBJDIR)/*.o