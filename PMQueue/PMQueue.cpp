// PMQueue.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#define DEBUG
#include "log.h"
#include "MessageQueue.h"
#include "Writer.h"
#include "Reader.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <cstdlib>
using namespace std;
/********************************************************
****************TEST BRANCH PARAMS***********************
*********************************************************/
constexpr unsigned int PRIORITY_COUNT     = 1;
constexpr unsigned int MESSAGE_SIZE_MIN   = 20;
constexpr unsigned int MESSAGE_SIZE_MAX   = 21;
constexpr unsigned int QUEUE_SIZE_MIN     = 2;
constexpr unsigned int QUEUE_SIZE_MAX     = 100;
constexpr auto TIME_SCALE                 = 1ns;
constexpr unsigned int READERS_COUNT      = 4;
constexpr unsigned int WRITERS_COUNT      = 6;
/*********************************************************/
unsigned int MESSAGE_SIZE = MESSAGE_SIZE_MIN;
unsigned int QUEUE_SIZE   = QUEUE_SIZE_MIN;
unsigned int HWL = QUEUE_SIZE * 9 / 10;
unsigned int LWL = QUEUE_SIZE / 10;
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
	int priority = rand() % PRIORITY_COUNT;

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
	g_table[msg.priority].first++;
	g_table[msg.priority].second += interval;
	//std::this_thread::sleep_for(TIME_SCALE);
}

void print_header(ostream& os) {
	os << "Test #,QUEUE SIZE,MESSAGE SIZE,LWL,HWL,Priority,Tokens count,Mean await time(ns)," << endl;
}
void print_profiler_result(ostream& os, int test_no) {
	for (unsigned int i = 0u; i < g_table.size(); i++) {
		const auto& e = g_table[i];
		os << test_no<<","<< QUEUE_SIZE << "," << MESSAGE_SIZE 
		   << "," << LWL << "," << HWL << "," << i << "," << e.first << ",";
		if (e.first > 0) {
			os << e.second.count() / e.first << ",";
		}
		os << endl;
	}
}
void reset_profiler_result(){
	for (unsigned int i = 0u; i < g_table.size(); i++) {
		g_table[i].first = 0;
		g_table[i].second = 0s;
	}
}

int main()
{
	ofstream out;
	out.open("output.csv");
	print_header(out);
	int test_no = 0;
	for(MESSAGE_SIZE = MESSAGE_SIZE_MIN; MESSAGE_SIZE<MESSAGE_SIZE_MAX; MESSAGE_SIZE <<= 2)
	 for(QUEUE_SIZE= QUEUE_SIZE_MIN;QUEUE_SIZE<QUEUE_SIZE_MAX;QUEUE_SIZE++)
	 {
		HWL = QUEUE_SIZE * 9 / 10;
		LWL = QUEUE_SIZE / 10;
		using MSG = Message;
		using MQ = MessageQueue<MSG>;
		using W = Writer<MSG>;
		using R = Reader<MSG>;
		MQ q{ QUEUE_SIZE,HWL,LWL };
		
		vector<unique_ptr<W>> w_vector;
		vector<unique_ptr<R>> r_vector;
		for (int i = 0; i < WRITERS_COUNT; i++)
			w_vector.emplace_back(new W(q, &token_generator));
		for (int i = 0; i < READERS_COUNT; i++)
			r_vector.emplace_back(new R(q, &token_handler));

		std::this_thread::sleep_for(1s);
		q.start();
		vector<std::thread> t_vector(WRITERS_COUNT + READERS_COUNT);
		for (int i = 0; i < (WRITERS_COUNT + READERS_COUNT); i++) {
			if (i < WRITERS_COUNT)
				t_vector[i] = std::thread(&W::run, w_vector[i].get());
			else
				t_vector[i] = std::thread(&R::run, r_vector[i- WRITERS_COUNT].get());
		}
		std::this_thread::sleep_for(5s);
		q.stop();
		for (int i = 0; i < (WRITERS_COUNT + READERS_COUNT); i++) {
			t_vector[i].join();
			log_debug("thread joined");
		}
		print_profiler_result(out, test_no++);
		reset_profiler_result();
	}
	cout << "AFTER ALL" << endl;
}

