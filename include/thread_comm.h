/*
 * thread_comm.h
 */

#pragma once

#include <iostream>
#include <thread>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <vector>
#include <unordered_set>
#include <chrono>

// Namespace thread_comm implements two simple class templates
// that can be used for communication between threads. The first
// class template is circular_queue which provides a circular
// one-way queue with limited capacity. Circular queues does not
// have the concept of ownership. Any thread that has access to
// it may perform read or write operations with it. The intended
// way for its usage, on the other hand, expects a producer
// remain a producer and a consumer remain a consumer. Otherwise
// a sender may read its own message, nullifying the purpose of
// it.
// The other class template is channel which provides a two-way,
// reusable communication medium between threads. A channel can
// have multiple producers and consumers on both ends. It can
// even support having separate read and write owners.
namespace thread_comm {
template <typename T>
class circular_queue {
private:
	typedef struct timeout_data_s {
		std::chrono::system_clock::duration duration;
		bool timed_out;

		timeout_data_s(const std::chrono::system_clock::duration _duration =
						std::chrono::system_clock::duration(0)):
					duration(_duration),
					timed_out(false)
		{}
	} timeout_data;

	std::size_t size;

	std::size_t read_index;
	std::size_t write_index;
	std::size_t count;

	// Although I was tempted to use a shared_mutex (rw_lock) for this,
	// especially for the msg_count function, the answer below convinced
	// me otherwise:
	// https://stackoverflow.com/questions/34930855/condition-variable-and-shared-mutex
	// Quoting from the answer directly:
	// Class condition_variable provides a condition variable that can only wait
	// on an object of type unique_lock<mutex>, allowing maximum efficiency on
	// some platforms.
	std::unique_ptr<std::mutex> protector;
	std::unique_ptr<std::condition_variable> read_cond;
	std::unique_ptr<std::condition_variable> write_cond;

	std::vector<std::unique_ptr<T>> data;

	void _write(std::unique_ptr<T> & message,
			std::unique_lock<std::mutex> & ulock,
			timeout_data *td = nullptr) {
		if (!td) {
			write_cond->wait(ulock, [this] { return count < size; });
		} else {
			if (!write_cond->wait_for(ulock, td->duration,
					[this] { return count < size; }
			)) {
				td->timed_out = true;
				return;
			}
		}

		data[write_index++] = std::move(message);

		if (write_index == size) {
			write_index = 0;
		}

		++count;

		read_cond->notify_one();
	}

	std::unique_ptr<T> _read(std::unique_lock<std::mutex> & ulock,
			timeout_data *td = nullptr) {
		std::unique_ptr<T> m;
		if (!td) {
			read_cond->wait(ulock, [this] { return count > 0; });
		} else {
			if (!read_cond->wait_for(ulock, td->duration,
					[this] { return count > 0; })) {
				td->timed_out = true;
				return nullptr;
			}
		}

		m = std::move(data[read_index++]);

		if (read_index == size) {
			read_index = 0;
		}

		--count;

		write_cond->notify_one();

		return m;
	}

public:
	circular_queue(int _size = 1) :
		size(_size),
		read_index(0),
		write_index(0),
		count(0) {
		if (size == 0) {
			std::cerr << "thread_comm::circular_queue - size can not be zero"
					<< std::endl;
			std::abort();
		}

		protector = std::make_unique<std::mutex>();
		read_cond = std::make_unique<std::condition_variable>();
		write_cond = std::make_unique<std::condition_variable>();

		data = std::vector<std::unique_ptr<T>>(size);
	}

	circular_queue<T>& operator=(circular_queue<T> && cq) {
		size = cq.size;
		read_index = cq.read_index;
		write_index = cq.write_index;
		count = cq.count;

		protector = std::move(cq.protector);
		read_cond = std::move(cq.read_cond);
		write_cond = std::move(cq.write_cond);

		data = std::move(cq.data);

		return *this;
	}

	void write(std::unique_ptr<T> & message) {
		std::unique_lock<std::mutex> ulock(*protector);
		_write(message, ulock);
	}

	std::unique_ptr<T> read() {
		std::unique_lock<std::mutex> ulock(*protector);
		return _read(ulock);
	}

	bool timed_write(std::unique_ptr<T> & message,
			const std::chrono::system_clock::duration duration) {
		timeout_data td(duration);

		std::unique_lock<std::mutex> ulock(*protector);
		_write(message, ulock, &td);

		return !td.timed_out;
	}

	std::unique_ptr<T> timed_read(
			const std::chrono::system_clock::duration duration,
			bool & timed_out) {
		timeout_data td(duration);

		std::unique_lock<std::mutex> ulock(*protector);
		auto m = _read(ulock, &td);
		timed_out = td.timed_out;

		return m;
	}

	bool try_writing(std::unique_ptr<T> & message) {
		std::unique_lock<std::mutex> ulock(*protector);
		if (count < size) {
			_write(message, ulock);
			return true;
		}
		return false;
	}

	std::unique_ptr<T> try_reading() {
		std::unique_lock<std::mutex> ulock(*protector);
		if (count > 0) {
			return _read(ulock);
		}

		return nullptr;
	}

	std::size_t msg_count() {
		std::unique_lock<std::mutex> ulock(*protector);
		return count;
	}

	void operator<<(std::unique_ptr<T> & message) {
		write(message);
	}

	void operator>>(std::unique_ptr<T> & message) {
		message = read();
	}
}; // circular_queue

// global overloads for circular_queue - start
template <typename T>
void operator>>(std::unique_ptr<T> & message, circular_queue<T> & cq) {
	cq.write(message);
}

template <typename T>
void operator<<(std::unique_ptr<T> & message, circular_queue<T> & cq) {
	message = cq.read();
}
// global overloads for circular_queue - end

template <typename T>
class channel {
private:
	class thr_safe_set {
	private:
		std::unordered_set<std::thread::id> owner_ids;
		std::shared_mutex rw_mutex;

	public:
		thr_safe_set() {} // For the empty sets

		thr_safe_set(const std::thread::id _id) {
			owner_ids.insert(_id);
		}

		bool present(const std::thread::id _id) {
			std::shared_lock<std::shared_mutex> r_guard(rw_mutex);
			return owner_ids.find(_id) != owner_ids.end();
		}

		void add(const std::thread::id _id) {
			std::lock_guard<std::shared_mutex> w_guard(rw_mutex);
			owner_ids.insert(_id);
		}

		void remove(const std::thread::id _id) {
			std::lock_guard<std::shared_mutex> w_guard(rw_mutex);
			owner_ids.erase(_id);
		}
	}; // thr_safe_set

	thr_safe_set read_owners;
	thr_safe_set write_owners;

	thr_safe_set non_readers;
	thr_safe_set non_writers;

	circular_queue<T> worker_to_read_owner_queue;
	circular_queue<T> write_owner_to_worker_queue;

	void assert_read_allowance(const std::thread::id _id) {
		if (non_readers.present(_id)) {
			std::cerr << "A non-reader thread can not read anymore:"
					<< _id << std::endl;
			abort();
		}
	}

	void assert_write_allowance(const std::thread::id _id) {
		if (non_writers.present(_id)) {
			std::cerr << "A non-writer thread can not write anymore:"
					<< _id << std::endl;
			abort();
		}
	}

public:
	channel(int read_q_size = 1, int write_q_size = 0) :
		read_owners(std::this_thread::get_id()),
		write_owners(std::this_thread::get_id()),
		worker_to_read_owner_queue(read_q_size),
		write_owner_to_worker_queue(
				write_q_size == 0 ? read_q_size:write_q_size)
	{}

	channel<T> & operator=(channel<T> && c) {
		read_owners = std::move(c.read_owners);
		write_owners = std::move(c.write_owners);
		non_readers = std::move(c.non_readers);
		non_writers = std::move(c.non_writers);
		write_owner_to_worker_queue = std::move(c.write_owner_to_worker_queue);
		worker_to_read_owner_queue = std::move(c.worker_to_read_owner_queue);

		return *this;
	}

	std::unique_ptr<T> read() {
		const std::thread::id _id = std::this_thread::get_id();
		assert_read_allowance(_id);

		if (read_owners.present(_id)) {
			return worker_to_read_owner_queue.read();
		} else {
			return write_owner_to_worker_queue.read();
		}
	}

	void write(std::unique_ptr<T> & message) {
		const std::thread::id _id = std::this_thread::get_id();
		assert_write_allowance(_id);

		if (write_owners.present(_id)) {
			write_owner_to_worker_queue.write(message);
		} else {
			worker_to_read_owner_queue.write(message);
		}
	}

	void operator>>(std::unique_ptr<T> & message) {
		message = read();
	}

	void operator<<(std::unique_ptr<T> & message) {
		write(message);
	}

	std::unique_ptr<T> try_reading() {
		const std::thread::id _id = std::this_thread::get_id();
		assert_read_allowance(_id);

		if (read_owners.present(_id)) {
			return worker_to_read_owner_queue.try_reading();
		} else {
			return write_owner_to_worker_queue.try_reading();
		}
	}

	std::unique_ptr<T> timed_read(
			const std::chrono::system_clock::duration duration,
			bool & timed_out) {
		const std::thread::id _id = std::this_thread::get_id();
		assert_read_allowance(_id);

		if (read_owners.present(_id)) {
			return worker_to_read_owner_queue.timed_read(duration, timed_out);
		} else {
			return write_owner_to_worker_queue.timed_read(duration, timed_out);
		}
	}

	std::size_t read_msg_count() {
		if (read_owners.present(std::this_thread::get_id())) {
			return worker_to_read_owner_queue.msg_count();
		} else {
			return write_owner_to_worker_queue.msg_count();
		}
	}

	bool try_writing(std::unique_ptr<T> & message) {
		const std::thread::id _id = std::this_thread::get_id();
		assert_write_allowance(_id);

		if (write_owners.present(_id)) {
			return write_owner_to_worker_queue.try_writing(message);
		} else {
			return worker_to_read_owner_queue.try_writing(message);
		}
	}

	bool timed_write(std::unique_ptr<T> & message,
			const std::chrono::system_clock::duration duration) {
		const std::thread::id _id = std::this_thread::get_id();
		assert_write_allowance(_id);

		if (write_owners.present(_id)) {
			return write_owner_to_worker_queue.timed_write(message, duration);
		} else {
			return worker_to_read_owner_queue.timed_write(message, duration);
		}
	}

	std::size_t write_msg_count() {
		if (write_owners.present(std::this_thread::get_id())) {
			return write_owner_to_worker_queue.msg_count();
		} else {
			return worker_to_read_owner_queue.msg_count();
		}
	}

	void become_a_non_reader() {
		read_owners.remove(std::this_thread::get_id());
		non_readers.add(std::this_thread::get_id());
	}

	void become_a_non_writer() {
		write_owners.remove(std::this_thread::get_id());
		non_writers.add(std::this_thread::get_id());
	}

	void become_a_read_owner() {
		const std::thread::id _id = std::this_thread::get_id();
		assert_read_allowance(_id);
		read_owners.add(_id);
	}

	void become_a_write_owner() {
		const std::thread::id _id = std::this_thread::get_id();
		assert_write_allowance(_id);
		write_owners.add(_id);
	}
}; // channel

// global overloads for channel - start
template <typename T>
void operator>>(std::unique_ptr<T> & message, channel<T> & c) {
	c.write(message);
}

template <typename T>
void operator<<(std::unique_ptr<T> & message, channel<T> & c) {
	message = c.read();
}
// global overloads for channel - end
} // namespace thread_comm
