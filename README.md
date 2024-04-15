# Simple Chat Server

This is a simple chat server implemented in C using socket programming. It allows multiple clients to connect and exchange messages in a chat room-like environment.

## Usage

### Prerequisites

- Make sure you have a C compiler installed on your system.
- This program uses standard C libraries and should work on most Unix-like systems.

### Building the Server

1. Clone this repository to your local machine:

    ```bash
    git clone https://github.com/yonatanhb/chat-server.git
    ```

2. Navigate to the project directory:

    ```bash
    cd simple-chat-server
    ```

3. Compile the server program using your preferred C compiler. For example, using `gcc`:

    ```bash
    gcc chatServer.c -o chatServer
    ```

### Running the Server

Once the server is built, you can run it with the following command:

```bash
./chatServer <port>
```

Replace `<port>` with the desired port number (e.g., `./chatServer 8080`). The server will start listening for incoming connections on the specified port.

### Connecting Clients

Clients can connect to the server using any TCP/IP client application, such as Telnet or Netcat. For example:

```bash
telnet localhost <port>
```

Replace `<port>` with the same port number used to start the server.

### Exiting the Server

To stop the server, press `Ctrl + C` in the terminal where the server is running. This will gracefully shut down the server and close all active connections.

## Contributing

Contributions are welcome! If you find any issues or have suggestions for improvements, please feel free to open an issue or submit a pull request.

---
