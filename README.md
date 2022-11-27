# cpp_channels
A humble c++ (c++17 and onwards) header-only, thread-safe channel
implementation that provides a similar functionality to golang channels.

A simple example would much more than I could explain in a long text:

```c++
#include <thread_comm.h>
#include <thread>
#include <memory>
#include <iostream>

// The 'worker' thread. This one is not an 'owner' of the
// channel.
void thread_main(thread_comm::channel<char> & c) {
    // Creating a message buffer, but not initializing it.
    // So, it's a null pointer. Please notice that the messages
    // are unique pointers to avoid unnecessary copy overheads.
    std::unique_ptr<char> msg;

    // Receiving a message (the area allocated by the main thread
    // from the heap pointed by a unique pointer).
    msg << c;

    std::cout << "Inside worker thread - msg: " << *msg << std::endl;

    // Making a modification in the message buffer.
    *msg = 'B';

    // Sending the same message buffer back to the 'owner'
    // thread.
    c << msg;
}

int main() {
    // Declaring a duplex channel that can hold one undelivered message
    // in both directions (These can be specified, and they should be
    // positive integers). The data that can be transferred by this channel
    // will have to be of type 'char'. Since this channel is declared in
    // the context of the main thread, its 'owned' by the main thread, and
    // the main thread will stay as the owner of it, unless we tamper with
    // the ownership.
    thread_comm::channel<char> c;

    // Starting the worker thread. Please notice that the channel
    // is passed by reference.
    std::thread t(thread_main, std::ref(c));

    // Declaring a unique pointer for the message, initializing it with
    // an allocation and writing 'A' into that allocated buffer.
    auto msg = std::make_unique<char>('A');

    // Sending the unique pointer msg over the channel c. The pointer
    // msg is NULL after this operation, as sending it over the channel
    // causes an std::move() operation.
    c << msg;

    // Receiving the same unique pointer with 'B' written in it.
    msg << c;

    std::cout << "Inside owner thread - msg: " << *msg << std::endl;

    // Waiting for the 'worker' thread.
    t.join();

    return 0;
}
```

The program above prints the lines below:

```
Inside worker thread - msg: A
Inside owner thread - msg: B
```

Here, I feel the urge to put some emphasis on the terms `owner thread` and
`worker thread`. An owner thread is by default the creator of the channel,
and a worker thread is any other thread that interacts with the channel.
Since a channel basically consists of two one-way circular queues, determination
of which queue a read or a write operation will be interacting with would
be ambiguous without these definitions. A worker thread writes into the queue
that the owner would be reading from and naturally, a worker thread reads from
the queue that the owner thread writes into. Unless the ownership is tampered
with, this mechanism makes sure that the creator of a channel will be able to
exchange messages with its child threads and the child threads wouldn't be
sending messages to each other by accident.

In some scenarios though, some threads may need to assume the ownership roles
of the creator thread (these can both be read or write ownerships). For such
scenarios the member functions `become_a_read_owner()`, `become_a_write_owner()`,
`become_a_non_reader()` and `become_a_non_writer()` of the channel can be used.

- The function become_a_read_owner() will make the calling worker thread another
read owner of the channel, so that when it tries to read data from that channel,
the data will be coming from the worker threads, not the other owner threads.

- The function become_a_write_owner() will make the calling worker thread
another write owner of that channel, so that when it tries to write into that
channel, the data will be sent to the worker threads, not the other owner
threads.

- The function become_a_non_reader() prevents the calling thread (worker or
owner) from performing further read operations on that channel. So when a read
owner calls this function, it's assumed to be giving up its rights to read from
that channel (another `collector` thread needs to run become_a_read_owner() to
be able to consume the messages that are coming from the worker threads in that
scenario). If a worker thread calls become_a_non_reader(), then it will only
be able to write into that channel. Any attempt to perform a read operation
will result with a SIGABRT.

- Lastly, the function become_a_non_writer prevents the calling thread (again,
both owner or worker threads can make this call) from performing further write
operations on that channel. When a write owner executes this function, it gives
up on its rights to write into that channel (in such a scenario, introducing a
new `producer` thread and making it issue become_a_write_owner() could be quite
beneficial). When a worker thread issues this call, it also forbids itself from
writing into this channel.

The four functions above are meant to be used for advanced pipelining scenarios,
and they are mostly unnecessary for the majority of the normal use cases.

Finally, special thanks to my good friend Korcan Ucar (https://github.com/kucar)
for reviewing and testing the header file.
