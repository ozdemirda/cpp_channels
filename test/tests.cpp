#include <iostream>
#include <gtest/gtest.h>
#include <thread_comm.h>
#include <string>
#include <chrono>
#include <vector>
#include <algorithm>

const int __base_sleep_msecs = 10;
// Giving ourselves some buffer, as timings can vary (especially with valgrind).
const int unblocked_msecs = 2;
const int sleep_msecs = __base_sleep_msecs + unblocked_msecs;
const int check_msecs = __base_sleep_msecs;

// Circular_Queue tests start here.

TEST(TestThreadComm, CircularQueue_BasicFunctionality) {
	thread_comm::circular_queue<char> cq_main2thr;

	std::thread t([&cq_main2thr](){
		std::unique_ptr<char> buf;
		EXPECT_EQ(buf, nullptr);

		buf << cq_main2thr;
		EXPECT_EQ(*buf, 'A');

		cq_main2thr >> buf;
		EXPECT_EQ(*buf, 'B');
	});

	auto mbuf = std::make_unique<char>('A');
	cq_main2thr << mbuf;

	mbuf = std::make_unique<char>('B');
	mbuf >> cq_main2thr;

	t.join();
}

TEST(TestThreadComm, CircularQueue_AbortForSizeZero) {
	EXPECT_EXIT(thread_comm::circular_queue<char> cq_main2thr(0),
			testing::KilledBySignal(SIGABRT), "");
}

TEST(TestThreadComm, CircularQueue_BasicFunctionalityReverse) {
	thread_comm::circular_queue<char> cq_thr2main;

	std::thread t([&cq_thr2main](){
		auto buf = std::make_unique<char>('A');
		cq_thr2main << buf;

		buf = std::make_unique<char>('B');
		buf >> cq_thr2main;
	});

	std::unique_ptr<char> mbuf;
	EXPECT_EQ(mbuf, nullptr);

	mbuf << cq_thr2main;
	EXPECT_EQ(*mbuf, 'A');

	cq_thr2main >> mbuf;
	EXPECT_EQ(*mbuf, 'B');

	t.join();
}

TEST(TestThreadComm, CircularQueue_ReadFromEmptyQueueBlocks) {
	thread_comm::circular_queue<char> cq_main2thr;

	std::thread t([&cq_main2thr] () {
		std::unique_ptr<char> buf;
		EXPECT_EQ(buf, nullptr);

		auto t1 = std::chrono::system_clock::now();
		buf << cq_main2thr;
		auto t2 = std::chrono::system_clock::now();
		auto dur = t2 - t1;

		EXPECT_TRUE(dur >= std::chrono::milliseconds(check_msecs));
		EXPECT_EQ(*buf, 'A');
	});

	auto mbuf = std::make_unique<char>('A');

	std::this_thread::sleep_for(std::chrono::milliseconds(sleep_msecs));

	cq_main2thr << mbuf;

	t.join();
}

TEST(TestThreadComm, CircularQueue_TryReadFromEmptyQueueDoesntBlock) {
	thread_comm::circular_queue<char> cq_main2thr;

	std::thread t([&cq_main2thr] () {
		std::unique_ptr<char> buf;
		EXPECT_EQ(buf, nullptr);

		auto t1 = std::chrono::system_clock::now();
		buf = cq_main2thr.try_reading();
		auto t2 = std::chrono::system_clock::now();
		auto dur = t2 - t1;
		EXPECT_TRUE(dur <= std::chrono::milliseconds(unblocked_msecs));
		EXPECT_EQ(buf, nullptr);

		t1 = std::chrono::system_clock::now();
		buf << cq_main2thr;
		t2 = std::chrono::system_clock::now();
		dur = t2 - t1;

		EXPECT_TRUE(dur >= std::chrono::milliseconds(check_msecs));
		EXPECT_EQ(*buf, 'A');
	});

	auto mbuf = std::make_unique<char>('A');

	std::this_thread::sleep_for(std::chrono::milliseconds(sleep_msecs));

	cq_main2thr << mbuf;

	t.join();
}

TEST(TestThreadComm, CircularQueue_TimedReadFromEmptyQueueBlocks) {
	thread_comm::circular_queue<char> cq_main2thr;

	std::thread t([&cq_main2thr] () {
		std::unique_ptr<char> buf;
		EXPECT_EQ(buf, nullptr);

		bool timed_out = false;

		auto t1 = std::chrono::system_clock::now();
		buf = cq_main2thr.timed_read(std::chrono::milliseconds(5), timed_out);
		auto t2 = std::chrono::system_clock::now();
		auto dur = t2 - t1;

		EXPECT_TRUE(dur >= std::chrono::milliseconds(5));
		EXPECT_EQ(buf, nullptr);
		EXPECT_TRUE(timed_out);

		t1 = std::chrono::system_clock::now();
		buf = cq_main2thr.timed_read(std::chrono::milliseconds(25), timed_out);
		t2 = std::chrono::system_clock::now();
		dur = t2 - t1;

		EXPECT_TRUE(dur < std::chrono::milliseconds(20));
		EXPECT_FALSE(timed_out);
		EXPECT_EQ(*buf, 'A');
	});

	auto mbuf = std::make_unique<char>('A');

	std::this_thread::sleep_for(std::chrono::milliseconds(20));

	cq_main2thr << mbuf;

	t.join();
}

TEST(TestThreadComm, CircularQueue_SuccessiveMessages) {
	thread_comm::circular_queue<char> cq_main2thr;

	std::thread t([&cq_main2thr](){
		std::unique_ptr<char> buf;
		EXPECT_EQ(buf, nullptr);

		buf << cq_main2thr;
		EXPECT_EQ(*buf, 'A');

		buf << cq_main2thr;
		EXPECT_EQ(*buf, 'B');
	});

	auto mbuf = std::make_unique<char>('A');
	cq_main2thr << mbuf;

	mbuf = std::make_unique<char>('B');
	cq_main2thr << mbuf;

	t.join();
}

TEST(TestThreadComm, CircularQueue_Size_1) {
	thread_comm::circular_queue<char> cq_main2thr(1);

	std::thread t([&cq_main2thr](){
		std::unique_ptr<char> buf;
		EXPECT_EQ(buf, nullptr);

		std::this_thread::sleep_for(std::chrono::milliseconds(sleep_msecs));

		buf << cq_main2thr;
		EXPECT_EQ(*buf, 'A');

		buf << cq_main2thr;
		EXPECT_EQ(*buf, 'B');
	});

	auto mbuf = std::make_unique<char>('A');
	auto t1 = std::chrono::system_clock::now();
	cq_main2thr << mbuf;
	auto t2 = std::chrono::system_clock::now();

	auto dur = t2 - t1;
	// We were NOT blocked while writing the first message,
	// Even 1 millisecond is quite long, but with valgrind,
	// it may take longer. So, just to be on the safe side.
	EXPECT_TRUE(dur <= std::chrono::milliseconds(unblocked_msecs));

	mbuf = std::make_unique<char>('B');
	t1 = std::chrono::system_clock::now();
	cq_main2thr << mbuf;
	t2 = std::chrono::system_clock::now();
	dur = t2 - t1;
	// We were blocked while writing the second message
	EXPECT_TRUE(dur >= std::chrono::milliseconds(check_msecs));

	t.join();
}

TEST(TestThreadComm, CircularQueue_Size_2) {
	thread_comm::circular_queue<char> cq_main2thr(2);

	std::thread t([&cq_main2thr](){
		std::unique_ptr<char> buf;

		std::this_thread::sleep_for(std::chrono::milliseconds(sleep_msecs));

		buf << cq_main2thr;
		EXPECT_EQ(*buf, 'A');

		buf << cq_main2thr;
		EXPECT_EQ(*buf, 'B');

		buf << cq_main2thr;
		EXPECT_EQ(*buf, 'C');
	});

	auto mbuf = std::make_unique<char>('A');
	auto t1 = std::chrono::system_clock::now();
	cq_main2thr << mbuf;
	auto t2 = std::chrono::system_clock::now();

	auto dur = t2 - t1;
	// We were NOT blocked while writing the first message,
	// Even 1 millisecond is quite long, but with valgrind,
	// it may take longer. So, just to be on the safe side.
	EXPECT_TRUE(dur <= std::chrono::milliseconds(unblocked_msecs));

	mbuf = std::make_unique<char>('B');
	t1 = std::chrono::system_clock::now();
	cq_main2thr << mbuf;
	t2 = std::chrono::system_clock::now();
	dur = t2 - t1;
	// We were NOT blocked.
	EXPECT_TRUE(dur <= std::chrono::milliseconds(unblocked_msecs));

	mbuf = std::make_unique<char>('C');
	t1 = std::chrono::system_clock::now();
	cq_main2thr << mbuf;
	t2 = std::chrono::system_clock::now();
	dur = t2 - t1;
	// We were blocked while writing the second message
	EXPECT_TRUE(dur >= std::chrono::milliseconds(check_msecs));

	t.join();
}

TEST(TestThreadComm, CircularQueue_MsgCount) {
	thread_comm::circular_queue<char> cq_main2thr(5);

	EXPECT_EQ(cq_main2thr.msg_count(), (std::size_t)0);

	std::thread t([&cq_main2thr] () {
		// Read three messages right away to have some 'movement'
		// in the circular buffer.
		std::unique_ptr<char> msg;
		msg << cq_main2thr;
		msg << cq_main2thr;
		msg << cq_main2thr;

		// Sleep for some time to give the opportunity to
		// the producer to fill the buffer.
		std::this_thread::sleep_for(std::chrono::milliseconds(sleep_msecs));

		// We expect the buffer to be full, and as we read,
		// we expect the message count to go down to zero.

		EXPECT_EQ(cq_main2thr.msg_count(), (std::size_t)5);
		msg << cq_main2thr;
		EXPECT_EQ(cq_main2thr.msg_count(), (std::size_t)4);
		msg << cq_main2thr;
		EXPECT_EQ(cq_main2thr.msg_count(), (std::size_t)3);
		msg << cq_main2thr;
		EXPECT_EQ(cq_main2thr.msg_count(), (std::size_t)2);
		msg << cq_main2thr;
		EXPECT_EQ(cq_main2thr.msg_count(), (std::size_t)1);
		msg << cq_main2thr;
		EXPECT_EQ(cq_main2thr.msg_count(), (std::size_t)0);
	});

	EXPECT_EQ(cq_main2thr.msg_count(), (std::size_t)0);

	// Write three messages to be read directly to move
	// the read and write indices.
	auto mmsg = std::make_unique<char>('A');
	cq_main2thr << mmsg;
	cq_main2thr << mmsg;
	cq_main2thr << mmsg;

	// Wait for the messages to be consumed
	while (cq_main2thr.msg_count() > 0) {
		std::this_thread::sleep_for(std::chrono::microseconds(100));
	}

	// Now we can fill the message buffer, as the consumer
	// is going to sleep for some time. We also expect the
	// message count to go up with every message that hasn't
	// been consumed.

	cq_main2thr << mmsg;
	EXPECT_EQ(cq_main2thr.msg_count(), (std::size_t)1);

	cq_main2thr << mmsg;
	EXPECT_EQ(cq_main2thr.msg_count(), (std::size_t)2);

	cq_main2thr << mmsg;
	EXPECT_EQ(cq_main2thr.msg_count(), (std::size_t)3);

	cq_main2thr << mmsg;
	EXPECT_EQ(cq_main2thr.msg_count(), (std::size_t)4);

	cq_main2thr << mmsg;
	EXPECT_EQ(cq_main2thr.msg_count(), (std::size_t)5);

	t.join();

	// As the consumer thread came up and read all of the
	// messages, we expect the message count to be zero.
	EXPECT_EQ(cq_main2thr.msg_count(), (std::size_t)0);
}

TEST(TestThreadComm, CircularQueue_TryWriteDoesntBlockWhenQIsFull) {
	thread_comm::circular_queue<char> cq_main2thr(1);

	std::thread t([&cq_main2thr] () {
		std::this_thread::sleep_for(std::chrono::milliseconds(sleep_msecs));
		std::unique_ptr<char> m = cq_main2thr.read();
		EXPECT_EQ(*m, 'A');
	});

	auto mmsg = std::make_unique<char>('A');

	auto t1 = std::chrono::system_clock::now();
	bool success = cq_main2thr.try_writing(mmsg);
	auto t2 = std::chrono::system_clock::now();
	auto dur = t2 - t1;
	EXPECT_TRUE(dur <= std::chrono::milliseconds(unblocked_msecs));
	EXPECT_TRUE(success);

	t1 = std::chrono::system_clock::now();
	success = cq_main2thr.try_writing(mmsg);
	t2 = std::chrono::system_clock::now();
	EXPECT_TRUE(dur <= std::chrono::milliseconds(unblocked_msecs));
	EXPECT_FALSE(success);

	t.join();
}

TEST(TestThreadComm, CircularQueue_TimedWriteBlocksWhenQIsFull) {
	thread_comm::circular_queue<char> cq_main2thr(1);

	std::thread t([&cq_main2thr] () {
		std::this_thread::sleep_for(std::chrono::milliseconds(15));
		std::unique_ptr<char> m = cq_main2thr.read();
		EXPECT_EQ(*m, 'A');
	});

	auto mmsg = std::make_unique<char>('A');

	auto t1 = std::chrono::system_clock::now();
	bool success = cq_main2thr.timed_write(mmsg, std::chrono::milliseconds(15));
	auto t2 = std::chrono::system_clock::now();
	auto dur = t2 - t1;
	EXPECT_TRUE(dur <= std::chrono::milliseconds(2));
	EXPECT_TRUE(success);

	t1 = std::chrono::system_clock::now();
	success = cq_main2thr.timed_write(mmsg, std::chrono::milliseconds(60));
	t2 = std::chrono::system_clock::now();
	EXPECT_TRUE(dur <= std::chrono::milliseconds(15));
	EXPECT_TRUE(success);

	t.join();
}

// Channel tests start here.
TEST(TestThreadComm, Channel_BasicFunctionality) {
	thread_comm::channel<char> c;

	std::thread t([&c](){
		std::unique_ptr<char> buf;
		EXPECT_EQ(buf, nullptr);

		buf << c;
		EXPECT_EQ(*buf, 'A');

		// Altering the message content from 'A' to 'B'
		*buf = 'B';

		// Sending the same message buffer back.
		c << buf;

		// Let's read again.
		c >> buf;
		EXPECT_EQ(*buf, 'C');

		*buf = 'D';
		buf >> c;
	});

	auto mbuf = std::make_unique<char>('A');

	c << mbuf;
	mbuf << c;
	EXPECT_EQ(*mbuf, 'B');

	*mbuf = 'C';
	mbuf >> c;

	c >> mbuf;
	EXPECT_EQ(*mbuf, 'D');

	t.join();
}

TEST(TestThreadComm, Channel_DifferentQSizes) {
	thread_comm::channel<char> c(1, 2);
	// 1 for worker -> read_owner, 2 for write_owner -> worker

	std::thread t([&c](){
		std::this_thread::sleep_for(std::chrono::milliseconds(sleep_msecs));

		// From the thread's perspective the read q will
		// have the messages.
		EXPECT_EQ(c.read_msg_count(), (std::size_t)2);

		std::unique_ptr<char> buf;

		for (int i = 0 ; i < 3 ; ++i) {
			buf << c;
		}

		EXPECT_EQ(c.read_msg_count(), (std::size_t)0);

		auto t1 = std::chrono::system_clock::now();
		c << buf;
		auto t2 = std::chrono::system_clock::now();
		auto dur = t2 - t1;
		EXPECT_TRUE(dur <= std::chrono::milliseconds(unblocked_msecs));

		// From the thread's perspective, the messages
		// that are waiting are in the write q.
		EXPECT_EQ(c.write_msg_count(), (std::size_t)1);

		// Second write gets blocked
		t1 = std::chrono::system_clock::now();
		c << buf;
		t2 = std::chrono::system_clock::now();
		dur = t2 - t1;
		EXPECT_TRUE(dur >= std::chrono::milliseconds(check_msecs));
	});

	auto mbuf = std::make_unique<char>('A');

	EXPECT_EQ(c.write_msg_count(), (std::size_t)0);

	auto t1 = std::chrono::system_clock::now();
	c << mbuf;
	auto t2 = std::chrono::system_clock::now();
	auto dur = t2 - t1;
	EXPECT_TRUE(dur <= std::chrono::milliseconds(unblocked_msecs));
	EXPECT_EQ(c.write_msg_count(), (std::size_t)1);

	t1 = std::chrono::system_clock::now();
	c << mbuf;
	t2 = std::chrono::system_clock::now();
	dur = t2 - t1;
	EXPECT_TRUE(dur <= std::chrono::milliseconds(unblocked_msecs));
	EXPECT_EQ(c.write_msg_count(), (std::size_t)2);

	// Third write gets blocked
	t1 = std::chrono::system_clock::now();
	c << mbuf;
	t2 = std::chrono::system_clock::now();
	dur = t2 - t1;
	EXPECT_TRUE(dur >= std::chrono::milliseconds(check_msecs));

	std::this_thread::sleep_for(std::chrono::milliseconds(sleep_msecs));
	EXPECT_EQ(c.read_msg_count(), (std::size_t)1);

	for (int i = 0 ; i < 2 ; ++i) {
		mbuf << c;
	}

	t.join();
}

TEST(TestThreadComm, Channel_TestTryReadingFromEmptyQueues) {
	thread_comm::channel<int> c;

	std::thread t([&c]() {
		EXPECT_TRUE(c.try_reading() == nullptr);
	});

	EXPECT_TRUE(c.try_reading() == nullptr);

	t.join();
}

TEST(TestThreadComm, Channel_TestTryWriting) {
	thread_comm::channel<int> c;

	std::thread t([&c]() {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));

		auto m = c.read();
		EXPECT_TRUE(m != nullptr);
		EXPECT_EQ(*m, 3);

		*m = 5;
		EXPECT_TRUE(c.try_writing(m));
		EXPECT_FALSE(c.try_writing(m));
	});

	auto m = std::make_unique<int>(3);
	// First write attempt will succeed, as channel has one empty slot.
	EXPECT_TRUE(c.try_writing(m));
	// Second write attempt will fail, as channel doesn't have a empty slot.
	EXPECT_FALSE(c.try_writing(m));

	std::this_thread::sleep_for(std::chrono::milliseconds(20));

	m = c.read();
	EXPECT_TRUE(m != nullptr);
	EXPECT_EQ(*m, 5);

	t.join();
}

TEST(TestThreadComm, Channel_TestTimedReadFromEmptyQueues) {
	thread_comm::channel<int> c;

	std::thread t([&c]() {
		bool timed_out = false;
		auto t1 = std::chrono::system_clock::now();
		auto m = c.timed_read(std::chrono::milliseconds(10), timed_out);
		auto t2 = std::chrono::system_clock::now();
		auto dur = t2 - t1;

		EXPECT_TRUE(m == nullptr);
		EXPECT_TRUE(timed_out);
		EXPECT_TRUE(dur >= std::chrono::milliseconds(10));
	});

	bool timed_out = false;
	auto t1 = std::chrono::system_clock::now();
	auto m = c.timed_read(std::chrono::milliseconds(10), timed_out);
	auto t2 = std::chrono::system_clock::now();
	auto dur = t2 - t1;

	EXPECT_TRUE(m == nullptr);
	EXPECT_TRUE(timed_out);
	EXPECT_TRUE(dur >= std::chrono::milliseconds(10));

	t.join();
}

TEST(TestThreadComm, Channel_TestTryReadingFromNonEmptyQueues) {
	thread_comm::channel<int> c;

	auto m = std::make_unique<int>(5);
	c << m;

	std::thread t([&c]() {
		std::unique_ptr<int> m;

		m = c.try_reading();
		EXPECT_TRUE(m != nullptr);

		*m = 3;

		c << m;
	});

	while (!m) {
		m = c.try_reading();
		std::this_thread::sleep_for(std::chrono::microseconds(500));
	}

	EXPECT_TRUE(*m == 3);

	t.join();
}

TEST(TestThreadComm, Channel_TestTimedWrite) {
	thread_comm::channel<int> c;

	std::thread t([&c]() {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));

		// We read only once, the second message is intentionally left
		// in the channel. It's going to be deleted when the channel
		// destructor runs.
		auto m = c.read();

		EXPECT_TRUE(m != nullptr);
		EXPECT_EQ(*m, 3);

		*m = 5;

		EXPECT_TRUE(c.timed_write(m, std::chrono::microseconds(1)));
		EXPECT_FALSE(c.timed_write(m, std::chrono::microseconds(1)));
		m = std::make_unique<int>(7);
		EXPECT_TRUE(c.timed_write(m, std::chrono::milliseconds(20)));
	});

	auto m = std::make_unique<int>(3);
	// First write attempt will succeed, as channel has one empty slot.
	EXPECT_TRUE(c.timed_write(m, std::chrono::microseconds(1)));
	// Second write attempt will fail, as channel doesn't have a empty slot.
	EXPECT_FALSE(c.timed_write(m, std::chrono::microseconds(1)));
	// Third write attempt will succeed, as reader will awake and read.
	m = std::make_unique<int>(9);
	EXPECT_TRUE(c.timed_write(m, std::chrono::milliseconds(20)));

	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	m = c.read();
	EXPECT_TRUE(m != nullptr);
	EXPECT_EQ(*m, 5);

	t.join();
}

TEST(TestThreadComm, Channel_TestTimedReadFromNonEmptyQueues) {
	thread_comm::channel<int> c;

	auto m = std::make_unique<int>(5);
	c << m;

	std::thread t([&c]() {
		std::unique_ptr<int> m;

		bool timed_out = false;
		m = c.timed_read(std::chrono::microseconds(1), timed_out);
		EXPECT_FALSE(timed_out);
		EXPECT_TRUE(m != nullptr);

		*m = 3;

		c << m;
	});

	bool timed_out = true;

	do {
		m = c.timed_read(std::chrono::microseconds(500), timed_out);
	} while (timed_out);

	EXPECT_TRUE(*m == 3);
	EXPECT_FALSE(timed_out);

	t.join();
}

TEST(TestThreadComm, Channel_MultipleWorkerThreads) {
	thread_comm::channel<int> c;

	int worker_count = 4;
	std::vector<std::thread> workers;

	for (int i = 0 ; i < worker_count ; ++i) {
		workers.emplace_back([&c, worker_count](int i) {
			std::unique_ptr<int> m;
			m << c;
			int offset = *m;
			*m = offset + i + worker_count;
			c << m;
		},i);
	}

	int offset = 1;
	for (int i = 0 ; i < worker_count ; ++i) {
		// Send offset to workers, offset is 1.
		auto m = std::make_unique<int>(offset);
		m >> c;
	}

	std::vector<int> results;

	// Collecting the results from the threads
	for (int i = 0 ; i < worker_count ; ++i) {
		auto r = c.read();
		results.push_back(*r);
	}

	std::sort(results.begin(), results.end());

	for (int i = 0 ; i < worker_count ; ++i) {
		EXPECT_EQ(results[i], offset + i + worker_count);
	}

	for (int i = 0 ; i < worker_count ; ++i) {
		workers[i].join();
	}
}

TEST(TestThreadComm, Channel_MultipleWorkerThreads_ResultsShippedToACollector) {
	thread_comm::channel<int> c;

	int worker_count = 4;
	std::vector<std::thread> workers;

	for (int i = 0 ; i < worker_count ; ++i) {
		workers.emplace_back([&c, worker_count](int i) {
			std::unique_ptr<int> m;
			m << c;
			int offset = *m;
			*m = offset + i + worker_count;
			c << m;
		},i);
	}

	// The main thread has no further intentions for reading
	// from the channel. If we call a read from now on in the
	// main thread, library will call abort on behalf of us.
	c.become_a_non_reader();

	int offset = 1;
	std::thread collector([&c, worker_count, offset]() {
		c.become_a_read_owner();
		// The collector thread is a read owner now.
		// The results will be shipped to the collector
		// thread by the worker threads. As the main thread
		// has given up on its right to read, it shouldn't
		// call read anymore, otherwise a SIGABRT will be
		// raised.

		std::vector<int> results;

		// Collecting the results from the worker threads
		for (int i = 0 ; i < worker_count ; ++i) {
			auto r = c.read();
			results.push_back(*r);
		}

		// Sorting the results here, as they arrive
		// in a random manner.
		std::sort(results.begin(), results.end());

		for (int i = 0 ; i < worker_count ; ++i) {
			EXPECT_EQ(results[i], offset + i + worker_count);
		}
	});

	// Here, we are running in the context of the
	// main thread. Although we are not allowed to
	// perform a read on the channel, we can still
	// write to it.
	for (int i = 0 ; i < worker_count ; ++i) {
		// Send offset to workers, offset is 1.
		auto m = std::make_unique<int>(offset);
		m >> c;
	}

	for (int i = 0 ; i < worker_count ; ++i) {
		workers[i].join();
	}
	collector.join();
}

TEST(TestThreadComm, Channel_OneProducer_MultipleWorkers_OneCollector) {
	thread_comm::channel<int> c;

	// The main thread is giving up on its rights to write to
	// the channel, as it will create a producer thread to do
	// the writing. From now on, the main thread shouldn't try
	// to write into the channel, as that will eventually cause
	// generation of SIGABRT.
	c.become_a_non_writer();

	int worker_count = 4;
	std::vector<std::thread> workers;

	for (int i = 0 ; i < worker_count ; ++i) {
		workers.emplace_back([&c, worker_count](int i) {
			std::unique_ptr<int> m;
			m << c;
			int offset = *m;
			*m = offset + i + worker_count;
			c << m;
		},i);
	}

	// The main thread has no further intentions for reading
	// from the channel. If we call a read from now on in the
	// main thread, library will call abort on behalf of us.
	c.become_a_non_reader();

	int offset = 1;
	std::thread collector([&c, worker_count, offset]() {
		c.become_a_non_writer();
		c.become_a_read_owner();
		// The collector thread is a read owner now.
		// The results will be shipped to the collector
		// thread by the worker threads. As the main thread
		// has given up on its right to read, it shouldn't
		// call read anymore, otherwise a SIGABRT will be
		// raised.

		std::vector<int> results;

		// Collecting the results from the worker threads
		for (int i = 0 ; i < worker_count ; ++i) {
			auto r = c.read();
			results.push_back(*r);
		}

		// Sorting the results here, as they arrive
		// in a random manner.
		std::sort(results.begin(), results.end());

		for (int i = 0 ; i < worker_count ; ++i) {
			EXPECT_EQ(results[i], offset + i + worker_count);
		}
	});

	std::thread producer([&c, worker_count, offset]() {
		c.become_a_write_owner();
		c.become_a_non_reader();

		// Here, we are running in the context of the
		// producer thread. We have become a write owner,
		// so we can write into the channel as if we
		// were the creator of it. Please also notice that
		// we have given up on our rights to read from it
		// since our role doesn't require it.
		for (int i = 0 ; i < worker_count ; ++i) {
			// Send offset to workers, offset is 1.
			auto m = std::make_unique<int>(offset);
			m >> c;
		}
	});

	for (int i = 0 ; i < worker_count ; ++i) {
		workers[i].join();
	}
	collector.join();
	producer.join();
}

int main(int argc, char ** argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
