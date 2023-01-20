# Socket Programming: Flow control and loss recovery
A client-server application that compares images that are stored on two end systems.
- The client reads several images from disk, then opens a connection to the server and sends the images to it. Each image is transferred in a single packet. The provided images are small enough to fit in a single ethernet frame.
- When the server receives a packet from the client, it extracts the image data from the packet’s payload and compares it with all of the images that are stored locally on the server. It writes the name of the file that matches the image to a local file. This file can then be used to check that your implementation works as expected
Flow control and packet loss machanisms are implemented, along with a sliding window algorithm. 

## Setup
There is a Makefile with the options:
**make** - compiles both programs resulting in executable binaries "client" and "server"
**make all** - does the same as make without any parameter
**make clean** - deletes the executables and any temporary files (eg. *.o)
