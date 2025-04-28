# **TFTP Client and Server**
#### Author: `Dalibor Kříčka`
#### 2023, Brno
---

## **Task Description**
The task is to implement a client and server application for file transfer using TFTP (Trivial File Transfer Protocol), strictly following the corresponding RFC specification of the protocol.

---

## **Program Usage**
### **Running the TFTP Client**
The TFTP client is launched using the following command:

```
tftp-client -h hostname [-p port] [-f filepath] -t dest_filepath
```

where:
* **-h hostname** – is the domain name or IPv4 address of the server
* **-p port** – is the port of the remote server
    * if not set, the port is 69 by default
* **-f filepath** – the path to the file to be downloaded from the server (download)
    * if not set, it uploads the contents on standard input to the server (upload)
* **-t dest_filepath** –  the path to the file where the transferred data will be stored on the server/locally

Jednotlivé parametry programu mohou být zádávány v libovolném pořadí.

### **Running the TFTP server**
The TFTP server is launched using the following command:

```
tftp-server [-p port] root_dirpath
```

where:
* **-p port** – is the port on which incoming connections will be expected.
    * if not set, the port 69 by default
* **root dirpath** – the path to the server directory where files will be uploaded to/downloaded from

The parameters can be specified in any order.

### **Displaying Help**
The help message for the program can be displayed using:

```
tftp-client --help
tftp-server --help
```

---

## **Program Description**
TFTP server a klient pro přenos souborů přes internetovou síť. The programs implement functionality corresponding to
[RFC1350](https://www.rfc-editor.org/info/rfc1350),
[RFC2347](https://www.rfc-editor.org/info/rfc2347),
[RFC2348](https://www.rfc-editor.org/info/rfc2348),
[RFC2349](https://www.rfc-editor.org/info/rfc2349) a
[RFC1123](https://www.rfc-editor.org/info/rfc1123).

### **Extensions**
The client supports transfer options including _block size_, _timeout interval_, and _transfer size_. hese can be set manually in the source file _tftp-client.cpp_ within the _main_ function by assigning the desired values to the `option_info_t option_information`.

### **Limitations**
Text files sent in _netascii_ mode must be in Linux format (lines ending with _LF_ only) before transfer, since both the client and the server are implemented for Linux environments and it is assumed that text files on these systems are stored in this format.
When transferring files where lines end with _CR LF_, an incorrect conversion to _netascii_ may occur.

### **Environment**

The program was tested on the following operating systems:
* NixOS – the reference development environment for the project,
* Fedora LINUX 39.

Correct functionality on operating systems other than those listed above (e.g., Windows, MacOS) is not guaranteed.

### **Technologies used**
The project was implemented in C++ using the following libraries:
* **iostream** – defines objects for standard input and output (cout, cin)
* **string.h** – defines functions for working with char* (strings)
* **regex** – defines functions for working with regular expressions, used for validating the program's input arguments
* **sys/socket.h** and **arpa/inet.h** – provide functions, data structures, and macros for network communication, used for socket management, setting up server host information, and sending/receiving messages
* **signal.h** – defines functions and macros for handling system signals, used for capturing the interrupt signal (Ctrl-C)
* **unistd.h** – functions close() is used for closing sockets and fork() for creating parallel processes to communicate with multiple clients
* **fstream** – defines class for working with files
* **filesystem** – used for obtaining information about available disk space
* **netdb.h** – defines functions for network database operations, used for translating a hostname into an IP address

## **Files**

* obj/
* src/
    * tftp-client.cpp
    * tftp-communication.cpp
    * tftp-communication.hpp
    * tftp-structures.cpp
    * tftp-structures.hpp
    * tftp-server.cpp
* temp/
* Makefile
* manual.pdf
* README.md
