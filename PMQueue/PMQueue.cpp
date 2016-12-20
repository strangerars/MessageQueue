// PMQueue.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#define LOG_DEBUG
#include "log.h"
#include "MessageQueue.h"
#include "Writer.h"
#include "Reader.h"
//#include <iostream>
#include <fstream>
//#include <thread>
//#include <chrono>
//#include <cstdlib>
#include <string>
//#include <cmath>
//using namespace std;
/********************************************************
****************TEST BRANCH PARAMS***********************
*********************************************************/
constexpr unsigned int PRIORITY_COUNT_DEFAULT = 15;
constexpr unsigned int PRIORITY_COUNT_MIN = 1;
constexpr unsigned int PRIORITY_COUNT_MAX = 1000;
constexpr unsigned int MESSAGE_SIZE_DEFAULT = 50;
constexpr unsigned int MESSAGE_SIZE_MIN = 1;
constexpr unsigned int MESSAGE_SIZE_MAX = 50'000;
constexpr unsigned int QUEUE_SIZE_DEFAULT = 20;
constexpr unsigned int QUEUE_SIZE_MIN = 1;
constexpr unsigned int QUEUE_SIZE_MAX = 50'000;
constexpr unsigned int TIME_SCALE_MIN = 1;
constexpr unsigned int TIME_SCALE_MAX = 100;
constexpr         auto TIME_SCALE_DEFAULT    = 10s;
constexpr unsigned int READERS_COUNT_MIN = 1;
constexpr unsigned int READERS_COUNT_MAX = 100;
constexpr unsigned int READERS_COUNT_DEFAULT = 4;
constexpr unsigned int WRITERS_COUNT_MIN = 1;
constexpr unsigned int WRITERS_COUNT_MAX = 100;
constexpr unsigned int WRITERS_COUNT_DEFAULT = 6;
constexpr unsigned int REPEAT_FACTOR_MIN = 1;
constexpr unsigned int REPEAT_FACTOR_MAX = 1000;
constexpr unsigned int REPEAT_FACTOR_DEFAULT = 1;
/*********************************************************/
unsigned int PRIORITY_COUNT = PRIORITY_COUNT_DEFAULT;
unsigned int MESSAGE_SIZE   = MESSAGE_SIZE_DEFAULT;
unsigned int QUEUE_SIZE     = QUEUE_SIZE_DEFAULT;
auto TIME_SCALE             = TIME_SCALE_DEFAULT;
unsigned int READERS_COUNT  = READERS_COUNT_DEFAULT;
unsigned int WRITERS_COUNT  = WRITERS_COUNT_DEFAULT;
unsigned int REPEAT_FACTOR  = REPEAT_FACTOR_DEFAULT;

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
vector <pair<int, std::chrono::nanoseconds>> g_table;
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
	log_debug("* PRIORITY_COUNT="  + std::to_string(PRIORITY_COUNT));
	log_debug("* MESSAGE_SIZE="    + std::to_string(MESSAGE_SIZE));
	log_debug("* QUEUE_SIZE="      + std::to_string(QUEUE_SIZE));
	log_debug("* TEST_DURATION(sec)=" + std::to_string(TIME_SCALE.count()));
	log_debug("* READERS_COUNT="   + std::to_string(READERS_COUNT));
	log_debug("* WRITERS_COUNT="   + std::to_string(WRITERS_COUNT));
	log_debug("* REPEAT_FACTOR="   + std::to_string(REPEAT_FACTOR));
	log_debug("****************************************");
}

inline void print_header(ostream& os) {
	os << "Test #,TEST_DURATION,QUEUE SIZE,MESSAGE SIZE,LWL,HWL,Priority,Tokens count,Mean await time(ns)," << endl;
}
inline void print_profiler_result(ostream& os, int test_no) {
	for (unsigned int i = 0u; i < g_table.size(); i++) {
		const auto& e = g_table[i];
		os << test_no<<","<< TIME_SCALE.count()<<"," << QUEUE_SIZE << "," << MESSAGE_SIZE
		   << "," << LWL << "," << HWL << "," << i << "," << e.first << ",";
		if (e.first > 0) {
			os << e.second.count() / e.first << ",";
		}
		os << endl;
	}
}
inline void reset_profiler_result(){
	for (unsigned int i = 0u; i < PRIORITY_COUNT; i++) {
		g_table[i].first = 0;
		g_table[i].second = 0s;
	}
}
int parse_cmd_params(int, char *[]);
int main(int argc, char * argv[])
{
	if (parse_cmd_params(argc, argv) < 0)
	{
		log_debug("cmdline args parse error");
		return -1;
	}
	g_table.resize(PRIORITY_COUNT);
	print_test_params();
	auto now = std::chrono::system_clock::now();
	auto now_str = std::to_string((std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()));
	ofstream out;
	string filename("output_" + now_str + ".csv");
	out.open(filename);
	print_header(out);
	std::this_thread::sleep_for(min(TIME_SCALE, 5s));
	for (unsigned int test_no = 0u; test_no < REPEAT_FACTOR; test_no++)
	{
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
			for (unsigned int i = 0u; i < WRITERS_COUNT; i++)
				w_vector.emplace_back(new W(q, &token_generator));
			for (unsigned int i = 0u; i < READERS_COUNT; i++)
				r_vector.emplace_back(new R(q, &token_handler));

			q.start();
			vector<std::thread> t_vector(WRITERS_COUNT + READERS_COUNT);
			for (unsigned int i = 0u; i < (WRITERS_COUNT + READERS_COUNT); i++) {
				if (i < WRITERS_COUNT)
					t_vector[i] = std::thread(&W::run, w_vector[i].get());
				else
					t_vector[i] = std::thread(&R::run, r_vector[i - WRITERS_COUNT].get());
			}
			std::this_thread::sleep_for(TIME_SCALE);
			q.stop();
			for (unsigned int i = 0u; i < (WRITERS_COUNT + READERS_COUNT); i++) {
				t_vector[i].join();
				log_debug("thread joined");
			}
			print_profiler_result(out, test_no);
			reset_profiler_result();
		}
		log_debug("Test #" + std::to_string(test_no) + " finished");

		std::this_thread::sleep_for(min(TIME_SCALE,5s));
		log_debug("See result in " + filename);
	}
}


int parse_cmd_params(int argc, char * argv[]) {
	if (argc > 1) {
		try {
			unsigned int val = stoi(argv[1]);
			QUEUE_SIZE = min(max(QUEUE_SIZE_MIN, val), QUEUE_SIZE_MAX);
		}
		catch (...) {
			log_debug("Failed to parse QUEUE_SIZE");
			return -1;
		}
	}
	else {
		return 0;
	}
	if (argc > 2) {
		try {
			unsigned int val = stoi(argv[2]);
			MESSAGE_SIZE = min(max(MESSAGE_SIZE_MIN, val), MESSAGE_SIZE_MAX);
		}
		catch (...) {
			log_debug("Failed to parse MESSAGE_SIZE");
			return -1;
		}
	}
	else {
		return 0;
	}
	if (argc > 3) {
		try {
			unsigned int val = stoi(argv[3]);
			PRIORITY_COUNT = min(max(PRIORITY_COUNT_MIN, val), PRIORITY_COUNT_MAX);
		}
		catch (...) {
			log_debug("Failed to parse PRIORITY_COUNT");
			return -1;
		}
	}
	else {
		return 0;
	}
	if (argc > 4) {
		try {
			unsigned int val = stoi(argv[4]);
			TIME_SCALE = std::chrono::seconds(min(max(TIME_SCALE_MIN, val), TIME_SCALE_MAX));
		}
		catch (...) { log_debug("Failed to parse TIME_SCALE");  return -1; }
	}
	else { return 0; }

	if (argc > 5) {
		try {
			unsigned int val = stoi(argv[5]);
			REPEAT_FACTOR = min(max(REPEAT_FACTOR_MIN, val), REPEAT_FACTOR_MAX);
		}
		catch (...) { log_debug("Failed to parse REPEAT_FACTOR");  return -1; }
	}
	else { return 0; }

	if (argc > 6) {
		try {
			unsigned int val = stoi(argv[6]);
			WRITERS_COUNT = min(max(WRITERS_COUNT_MIN, val), WRITERS_COUNT_MAX);
		}
		catch (...) { log_debug("Failed to parse WRITERS_COUNT");  return -1; }
	}
	else { return 7; }

	if (argc > 6) {
		try {
			unsigned int val = stoi(argv[7]);
			READERS_COUNT = min(max(READERS_COUNT_MIN, val), READERS_COUNT_MAX);
		}
		catch (...) { log_debug("Failed to parseREADERS_COUNT");  return -1; }
	}
	else { return 0; }
	return 0;
}