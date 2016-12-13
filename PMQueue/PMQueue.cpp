// PMQueue.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#define LOG_DEBUG
#include "log.h"
#include "MessageQueue.h"
#include "Writer.h"
#include "Reader.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cstdlib>
using namespace std;
/********************************************************
****************TEST BRANCH PARAMS***********************
*********************************************************/
const unsigned int PRIORITY_COUNT     = 15;
const unsigned int MESSAGE_SIZE       = 2;
const unsigned int QUEUE_SIZE         = 2;
const unsigned int HWL                = QUEUE_SIZE * 9 /10;
const unsigned int LWL                = QUEUE_SIZE / 10;
const auto TIME_SCALE                 = 1ns;
/*********************************************************/

struct Message {
	int priority;
	std::chrono::steady_clock::time_point created;
	string s;
	friend ostream& operator<<(ostream& os, const Message& dt);
	Message(const Message&) = default;
	Message(Message&&) = default;
	Message() = default;
	Message& operator=(const Message&) = default;
	Message& operator=(Message&&) = default;

};

ostream& operator<<(ostream& os, const Message& msg)
{
	os << "Priority=" << msg.priority << endl << "MSG="<<msg.s.c_str()<<endl;
	return os;
}
pair<Message, int> token_generator() {
	int priority = 1 + rand() % PRIORITY_COUNT;

	std::string str;
	str.insert(0, MESSAGE_SIZE, '*');
	//std::this_thread::sleep_for(TIME_SCALE);
	Message msg;
	msg.priority = priority;
	msg.s = move(str);
	msg.created = std::chrono::steady_clock::now();
	return make_pair(move(msg), priority);
}
vector <pair<int, std::chrono::nanoseconds>> g_table(PRIORITY_COUNT);
void token_handler(const Message& msg) {
	auto now = std::chrono::steady_clock::now();
	auto interval = std::chrono::duration_cast<std::chrono::nanoseconds>(now - msg.created);
	g_table[msg.priority-1].first++;
	g_table[msg.priority-1].second += interval;
	//std::this_thread::sleep_for(TIME_SCALE);
}

void print_profiler_result() {
	cout << "*******************************************" << endl;
	cout << "*************PROFILER OUTPUT***************" << endl;
	cout << "*******************************************" << endl;
	for (int i = 0; i < g_table.size(); i++) {
		const auto& e = g_table[i];
		cout << "* Priority: " << i + 1 << endl;
		cout << "* Tokens count: " << e.first << endl;
		if (e.first > 0) {
			cout << "* Mean awaiting time: " << e.second.count() / e.first << "ns" << endl;
		}
		cout << "*******************************************" << endl;
	}
}

int main()
{
	{
		using MSG = Message;
		using MQ = MessageQueue<MSG>;
		using W = Writer<MSG>;
		using R = Reader<MSG>;
		MQ q{ QUEUE_SIZE,HWL,LWL };
		W w1(q, &token_generator);
		W w2(q, &token_generator);
		W w3(q, &token_generator);
		W w4(q, &token_generator);
		R r1(q, &token_handler);
		R r2(q, &token_handler);
		std::this_thread::sleep_for(1s);
		q.start();
		std::thread t1(&W::run, &w1);
		std::thread t2(&W::run, &w2);
		std::thread t3(&W::run, &w3);
		std::thread t4(&W::run, &w4);
		std::thread t5(&R::run, &r1);
		std::thread t6(&R::run, &r2);
		std::this_thread::sleep_for(5s);
		q.stop();
		
		t1.join();
		log_debug("T1");
		t2.join();
		log_debug("T2");
		t3.join();
		log_debug("T3");
		t4.join();
		log_debug("T4");
		t5.join();
		log_debug("T5");
		t6.join();
		log_debug("T6");
		
	}
	cout << "AFTER ALL" << endl;
	print_profiler_result();
}

