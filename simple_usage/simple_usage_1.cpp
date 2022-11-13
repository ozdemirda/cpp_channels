#include <thread_comm.h>
#include <thread>
#include <memory>

void thread_main(thread_comm::channel<int> & c) {
    while (true) {
        auto m = c.read();

        if (!m) {
            break;
        }

        c << m;
    }
}

int main() {
    thread_comm::channel<int> c;

    std::thread t(thread_main, std::ref(c));

    for (int i = 0 ; i < 1000000 ; ++i) {
        auto msg = std::make_unique<int>(i);
        c << msg;
        msg << c;
    }

    std::unique_ptr<int> m;
    c << m;

    t.join();
}
