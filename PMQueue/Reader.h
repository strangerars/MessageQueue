#pragma once
#include "RetCodes.h"
#include "MessageQueue.h"
#include <functional>
#include "log.h"
using namespace std;
template <typename T>
class Reader
{
	using F = function<void (T)>;
	F m_handle_functor = nullptr;
	MessageQueue<T>* m_q_ptr = nullptr;
	void handle_message(T&& arg) {
		m_handle_functor(move(arg));
	}

public:
	Reader(MessageQueue<T>& q, F func) {
		m_q_ptr = &q;
		m_handle_functor = func;
	}
	void run() {
		T a;
		RetCodes ret = RetCodes::OK;
		while (ret != RetCodes::STOPED) {
			ret = m_q_ptr->get(a);
			if(ret == RetCodes::OK) handle_message(move(a));
		}
		log_debug( "READER IS OVER");
	}

};

