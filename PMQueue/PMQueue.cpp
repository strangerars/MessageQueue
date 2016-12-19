// PMQueue.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#define LOG_DEBUG
#include "log.h"
#include "MessageQueue.h"
#include "Writer.h"
#include "Reader.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <string>
using namespace std;
/********************************************************
****************TEST BRANCH PARAMS***********************
*********************************************************/
constexpr unsigned int PRIORITY_COUNT = 15;
constexpr unsigned int MESSAGE_SIZE_MIN = 50;
constexpr unsigned int MESSAGE_SIZE_MAX = 51;
constexpr unsigned int QUEUE_SIZE_MIN = 20;
constexpr unsigned int QUEUE_SIZE_MAX = 21;
constexpr         auto TIME_SCALE    = 10s;
constexpr unsigned int READERS_COUNT = 4;
constexpr unsigned int WRITERS_COUNT = 6;
constexpr unsigned int REPEAT_FACTOR = 1;
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
}

inline void print_test_params() {
	log_debug("****************************************");
	log_debug("* Test parameters:");
	log_debug("****************************************");
	log_debug("* PRIORITY_COUNT=" + std::to_string(PRIORITY_COUNT));
	log_debug("* MESSAGE_SIZE_MIN=" + std::to_string(MESSAGE_SIZE_MIN));
	log_debug("* MESSAGE_SIZE_MAX=" + std::to_string(MESSAGE_SIZE_MAX));
	log_debug("* QUEUE_SIZE_MIN=" + std::to_string(QUEUE_SIZE_MIN));
	log_debug("* QUEUE_SIZE_MAX=" + std::to_string(QUEUE_SIZE_MAX));
	log_debug("* TIME_SCALE(sec)=" + std::to_string(TIME_SCALE.count()));
	log_debug("* READERS_COUNT=" + std::to_string(READERS_COUNT));
	log_debug("* WRITERS_COUNT=" + std::to_string(WRITERS_COUNT));
	log_debug("* REPEAT_FACTOR=" + std::to_string(REPEAT_FACTOR));
	log_debug("****************************************");
}

inline void print_header(ostream& os) {
	os << "Test #,QUEUE SIZE,MESSAGE SIZE,LWL,HWL,Priority,Tokens count,Mean await time(ns)," << endl;
}
inline void print_profiler_result(ostream& os, int test_no) {
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
inline void reset_profiler_result(){
	for (unsigned int i = 0u; i < g_table.size(); i++) {
		g_table[i].first = 0;
		g_table[i].second = 0s;
	}
}

int main()
{
	print_test_params();
	auto now = std::chrono::system_clock::now();
	auto now_str = std::to_string((std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()));
	ofstream out;
	out.open("output_" + now_str + ".csv");
	print_header(out);
	for (int test_no = 0; test_no < REPEAT_FACTOR; test_no++)
		for (MESSAGE_SIZE = MESSAGE_SIZE_MIN; MESSAGE_SIZE < MESSAGE_SIZE_MAX; MESSAGE_SIZE <<= 2)
			for (QUEUE_SIZE = QUEUE_SIZE_MIN; QUEUE_SIZE < QUEUE_SIZE_MAX; QUEUE_SIZE <<= 1)
			{
				std::this_thread::sleep_for(TIME_SCALE);
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

					q.start();
					vector<std::thread> t_vector(WRITERS_COUNT + READERS_COUNT);
					for (int i = 0; i < (WRITERS_COUNT + READERS_COUNT); i++) {
						if (i < WRITERS_COUNT)
							t_vector[i] = std::thread(&W::run, w_vector[i].get());
						else
							t_vector[i] = std::thread(&R::run, r_vector[i - WRITERS_COUNT].get());
					}
					std::this_thread::sleep_for(TIME_SCALE);
					q.stop();
					for (int i = 0; i < (WRITERS_COUNT + READERS_COUNT); i++) {
						t_vector[i].join();
						log_debug("thread joined");
					}
					print_profiler_result(out, test_no);
					reset_profiler_result();
				}
				log_debug("Test #" + std::to_string(test_no) + " finished");
			}
	std::this_thread::sleep_for(TIME_SCALE);
}

