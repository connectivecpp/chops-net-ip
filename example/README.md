# Building Examples

The easiest way to build all of the examples is to use CMake. CMake generates build scripts including makefiles which build unit tests and example apps. By default both unit tests and example apps are built.

For example (commands shown are for Linux or macOS, Windows commands would be similar but not the same):

>mkdir my-build
>cd my-build
>cmake ../chops-net-ip

This assumes that `my-build` is created parallel to the `chops-net-ip` directory. `my-build` could also be created elsewhere. The `cmake` command references the top-level `CMakeLists.txt` file in the `chops-net-ip` directory.

>make all

This builds the unit test and example apps.

>make test

This runs all of the unit tests. Most of the example apps are interactive and not meant to be run as a unit test.

>cd example

Run desired example app.

## Example Application Source Code

All of the example source code is located in the example folder of chops-net-ip. 

#### Dependencies
The following dependencies are required to build the example apps (an additional dependency, Catch2, is needed for building the unit tests).

* [Asio](https://github.com/chriskohlhoff/asio)
* [Utility Rack](https://github.com/connectivecpp/utility-rack)
* [Expected Lite](https://github.com/martinmoene/expected-lite)

# Building and Running Individual Example Apps

The following instructions are a guideline for building an example app from the command line. 

**Note**: For building on macOS add ```-Wno-unused-lambda-capture``` to the following compile commands.

## Chat Server
Example of TCP multichat server network program.
example/chat_server_demo.cpp

### Directions
1. Navigate to the example folder in your command terminal:
```
cd <path>/chops-net-ip/example
```
2. Build the example
g++ on pc or linux:
```
g++ -std=c++17 -Wall -Werror\
-I ../include \
-I <path to utility rack>/include/ \
-I <path to Asio>/include/ \
-I <path to expected-lite>/include/ \
simple_chat_demo.cpp -lpthread -o chat_server
```
Build the example with g++ or clang++ on OSX:
```
g++ -std=c++17 -Wall -Werror -Wno-unused-lambda-capture \
-I ../include \
-I <path to utility rack>/include/ \
-I <path to Asio>/include/ \
-I <path to expected-lite>/include/ \
simple_chat_demo.cpp -lpthread -o chat_server
```

3. Execute the file:
```
./chat_server
```
The server will be running on default port 5001.
Press enter/return to exit the server.
### Documentation
usage:
```
  ./chat_server [-h] [-e] [port]
      -h   print usage
      -e   print all error messages to console
      default port = 5001
```

## Echo Binary Text Client
TCP connector (client) that sends binary text message to a
 server, receives message back converted to upper case.

### Directions

1. Navigate to the example folder in your command terminal:
```
cd <path>/chops-net-ip/example
```
2. Build the example 

g++ or clang++ on pc or linux:
```
g++ -std=c++17 -Wall -Werror \
-I ../include \
-I <path>/utility-rack/include/ \
-I <path>/asio/asio/include/ \
-I <path>/expected-lite/include/ \
echo_binary_text_client_demo.cpp -lpthread -o echo_client
```
build with clang++ or g++ on OSX:
```
g++ -std=c++17 -Wall -Werror -Wno-unused-lambda-capture \
-I ../include \
-I <path>/utility-rack/include/ \
-I <path>/asio/asio/include/ \
-I <path>/expected-lite/include/ \
echo_binary_text_client_demo.cpp -lpthread -o echo_client
```
3. Execute the file:
```
./echo_client
```
The server will be running on default port 5002.

4. Exit the program
To exit type in 'quit' and press enter
or ctrl/cmd + c

### Documentation
useage: 
```
./echo_client [-h | -e] [ip address/hostname] [port]
  -h           Print useage
  -e           Print error messages
  ip address   Default: 127.0.0.1 (LOCAL LOOP)
  port         Default: 5002
  change port and use local loop:
    ./echo_client [-e] "" port
```

## Echo Binary Text Server
TCP acceptor (server) that receives binary text messages, converts
 to upper case, then echos back to TCP connector (client).
 ### Directions
 1. Navigate to the example folder in your command terminal:
```
cd <path>/chops-net-ip/example
```
2. Build the example
with g++ on pc or linux:
```
g++ -std=c++17 -Wall -Werror \
-I ../include \
-I <path>/utility-rack/include/ \
-I <path>/asio/asio/include/ \
-I <path>/expected-lite/include/ \
echo_binary_text_server_demo.cpp -lpthread -o echo_server
```
Build the example with g++ or clang++ on OSX:
```
g++ -std=c++17 -Wall -Werror -Wno-unused-lambda-capture \
-I ../include \
-I <path>/utility-rack/include/ \
-I <path>/asio/asio/include/ \
-I <path>/expected-lite/include/ \
echo_binary_text_server_demo.cpp -lpthread -o echo_server
```
3. Execute the file:
```
./chat_server
```
The server will be running on default port 5002.

4. To exit press ENTER/RETURN or CMD/CTRL + C

 ### Documentation
 useage:
 ```
./echo_server [-h | -e] [port]
  -h    Print useage
  -e    Print error messages
  port  Default: 5002
 ```

## Local Echo
Example of TCP send/receive text string over local loop network
 connection.

 ### Directions
 1. Navigate to the example folder in your command terminal:
```
cd <path>/chops-net-ip/example
```
2. Build the example
 with g++ on pc or linux:
 ```
 g++ -std=c++17 -Wall -Werror \
-I ../include \
-I <path>/utility-rack/include/ \
-I <path>/asio/asio/include/ \
-I <path>/expected-lite/include/ \
 local_echo_demo.cpp -lpthread -o local_echo
 ```
Build the example with g++ or clang++ on OSX:
 ```
 g++ -std=c++17 -Wall -Werror -Wno-unused-lambda-capture \
-I ../include \
-I <path>/utility-rack/include/ \
-I <path>/asio/asio/include/ \
-I <path>/expected-lite/include/ \
 local_echo_demo.cpp -lpthread -o local_echo
 ```
 3. Execute the file
 ```
 ./local_echo
 ```

 4. Exit the program
 To exit type in 'quit' and press enter
or CTRL/CMD + C

 ### Documentation

## Simple Chat
Example of TCP peer to peer network chat program.
 ### Directions
 1. Navigate to the example folder in your command terminal:
```
cd <path>/chops-net-ip/example
```
2. Build the example
 with g++ on pc or linux:
 ```
 g++ -std=c++17 -Wall -Werror \
-I ../include \
-I <path>/utility-rack/include/ \
-I <path>/asio/asio/include/ \
-I <path>/expected-lite/include/ \
 simple_chat_demo.cpp -lpthread -o chat
 ```
Build the example with g++ or clang++ on OSX:
 ```
 g++ -std=c++17 -Wall -Werror -Wno-unused-lambda-capture \
-I ../include \
-I <path>/utility-rack/include/ \
-I <path>/asio/asio/include/ \
-I <path>/expected-lite/include/ \
 simple_chat_demo.cpp -lpthread -o chat
 ```
 3. Execute the file
 For this demo you're going to need two command terminals open in the same example directory 
 In the first command terminal:
 ```
 ./chat -accept
 ```
 In the second command terminal:
 ```
 ./chat -connect
 ```
 You can write in either command terminal and see the result in the other command terminal.
 4. Exit the program
  To exit type in 'quit' and press enter
or CTRL/CMD + C

 ### Documentation
 usage: 
 ```
 ./chat [-h] [-e] -connect | -accept [ip address] [port]
  -h  print usage info
  -e  print error and diagnostic messages
  -connect  tcp_connector
  -accept   tcp_acceptor
  default ip address: 127.0.0.1 (local loop)
  default port: 5001
  if connection type = accept, IP address becomes ""
  ```

## UDP Broadcast
UDP broadcast demo. Text messages are sent to the local network
 UDP broadcast address.
### Directions
 1. Navigate to the example folder in your command terminal:
```
cd <path>/chops-net-ip/example
```
2. Build the example
 with g++ on pc or linux:
 ```
 g++ -std=c++17 -Wall -Werror \
-I ../include \
-I <path>/utility-rack/include/ \
-I <path>/asio/asio/include/ \
-I <path>/expected-lite/include/ \
 udp_broadcast_demo.cpp -lpthread -o udp_broad
 ```
Build the example with g++ or clang++ on OSX:
 ```
 g++ -std=c++17 -Wall -Werror -Wno-unused-lambda-capture \
-I ../include \
-I <path>/utility-rack/include/ \
-I <path>/asio/asio/include/ \
-I <path>/expected-lite/include/ \
 udp_broadcast_demo.cpp -lpthread -o udp_broad
 ```
 3. Execute the file
 In the first command terminal:
 ```
 ./udp_broad  <ip address>
 ```
 You can write in either command terminal and see the result in the other command terminal.
 4. Exit the program
  To exit type in 'quit' and press enter
or CTRL/CMD + C

 ### Documentation
 usage:
 ```
  ./udp_broad [-h]  Print useage
  ./udp_broad [-e] <IP address> [subnet mask] [port]
     -e             Print errors and system messages
     IP address     IP address of this machine
     subnet mask    Default: 255.255.255.0
     port           Default: 5005
  ./udp_broad [-e] -b <broadcast address> [port]
     -e             Print errors and system messages
     -b broadcast address
        known broadcast address for this machine
        ex: 192.168.1.255, 172.145.255.255, 
     port           Default: 5005
```

## UDP reciever 
UDP reciever demo. Receive text messages from UDP broadcast agent.
### Directions
 1. Navigate to the example folder in your command terminal:
```
cd <path>/chops-net-ip/example
```
2. Build the example
 with g++ on pc or linux:
 ```
 g++ -std=c++17 -Wall -Werror \
-I ../include \
-I <path>/utility-rack/include/ \
-I <path>/asio/asio/include/ \
-I <path>/expected-lite/include/ \
udp_receiver_demo.cpp -lpthread -o udp_receive
 ```
Build the example with g++ or clang++ on OSX:
 ```
 g++ -std=c++17 -Wall -Werror -Wno-unused-lambda-capture \
-I ../include \
-I <path>/utility-rack/include/ \
-I <path>/asio/asio/include/ \
-I <path>/expected-lite/include/ \
 udp_receiver_demo.cpp -lpthread -o udp_receive
 ```
 3. Execute the file
 In the first command terminal:
 ```
 ./udp_receive 
 ```
 You can write in either command terminal and see the result in the other command terminal.
 4. Exit the program
  Press ENTER/RETURN

 ### Documentation
 usage:
 ```
  ./udp_receive [-h] [-e] [port]
   -h      Print usage
   -e      Print error and system messages
   port    Default: 5005
```

## coming soon... udp multicast
