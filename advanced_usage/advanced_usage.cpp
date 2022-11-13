#include <thread_comm.h>
#include <thread>
#include <vector>

const int number_of_messages_base_unit = 100000;

const int number_of_producers = 4;
const int number_of_collectors = 4;
const int number_of_workers = 2*number_of_producers;

const int total_number_of_messages = number_of_producers*number_of_collectors*
		number_of_messages_base_unit;

void producer_thread_main(thread_comm::channel<int> & c) {
	c.become_a_write_owner();
	c.become_a_non_reader();

	const int number_of_messages_per_producer =
			total_number_of_messages/number_of_producers;

	for (int i = 0 ; i < number_of_messages_per_producer ; ++i) {
		auto msg = std::make_unique<int>(i);
		c << msg;
	}

	std::unique_ptr<int> msg;
	int final_msg_per_thread = number_of_workers/number_of_producers;
	for (int i = 0 ; i < final_msg_per_thread ; ++i) {
		c << msg;
	}
}

void worker_thread_main(thread_comm::channel<int> & c) {
	std::unique_ptr<int> msg;

	while (true) {
		msg << c;

		if (!msg) {
			break;
		}

		c << msg;
	}
}

void collector_thread_main(thread_comm::channel<int> & c) {
	c.become_a_read_owner();
	c.become_a_non_writer();

	std::unique_ptr<int> msg;

	int counter = 0;
	const int number_of_messages_per_collector =
			total_number_of_messages / number_of_collectors;

	while (true) {
		msg << c;

		if (++counter == number_of_messages_per_collector) {
			break;
		}
	}
}

int main() {
	thread_comm::channel<int> c(8192, 1024);
	c.become_a_non_reader();
	c.become_a_non_writer();

	std::vector<std::thread> producers;
	std::vector<std::thread> workers;
	std::vector<std::thread> collectors;

	for (int i = 0 ; i < number_of_collectors ; ++i) {
		collectors.emplace_back(collector_thread_main, std::ref(c));
	}

	for (int i = 0 ; i < number_of_workers ; ++i) {
		workers.emplace_back(worker_thread_main, std::ref(c));
	}

	for (int i = 0 ; i < number_of_producers ; ++i) {
		producers.emplace_back(producer_thread_main, std::ref(c));
	}

	// Wait for the work to be completed now.

	for (int i = 0 ; i < number_of_producers ; ++i) {
		producers[i].join();
	}

	for (int i = 0 ; i < number_of_workers ; ++i) {
		workers[i].join();
	}

	for (int i = 0 ; i < number_of_collectors ; ++i) {
		collectors[i].join();
	}

	std::cout << "Total number of messages: "
			<< total_number_of_messages << std::endl;

	return 0;
}
