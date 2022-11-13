#include <thread_comm.h>
#include <thread>
#include <memory>
#include <vector>
#include <string>

typedef struct some_data {
    int thread_id;
    float val;
} some_data_t;

void thread_main(thread_comm::channel<some_data_t> &c, int thread_id) {
    auto result = std::make_unique<some_data_t>();

    result->thread_id = thread_id;
    result->val = thread_id * 2;

    c << result;
}

int main() {
    thread_comm::channel<some_data_t> c;
    std::vector<std::thread> threads;

    int thread_count = 4;

    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back(thread_main, std::ref(c), i);
    }

    for (int i = 0; i < thread_count; ++i) {
        auto r = c.read();
        std::cout << "Got result: " << r->thread_id
                  << " - " << r->val << std::endl;
    }

    for (auto & t : threads) {
        t.join();
    }

    return 0;
}
