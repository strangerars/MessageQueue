#pragma once
#include "MessageQueue.h"
#include "Log.h"
#include "IMessageQueueEvents.h"
#include <iostream>
using namespace std;
template <typename T >
class Writer: private IMessageQueueEvents
{
	typedef function<pair<T,int>()> F;
private:
	MessageQueue<T>* m_q_ptr = nullptr;
	F m_functor = nullptr;
	std::mutex m_mutex;
	std::condition_variable m_sleeping_cond;
public:
	Writer(MessageQueue<T>& q, F func) {
		q.set_events(this);
		m_q_ptr = &q;
		m_functor = func;
	}
	void run() {
		while (m_current_state != STOPPED) {
			if (m_current_state == SLEEPING) {
				std::unique_lock<std::mutex> sleep_lock(m_mutex);
				while (m_current_state == SLEEPING) {
					m_sleeping_cond.wait(sleep_lock);
				}
				if (m_current_state == STOPPED) break;
			};

			auto val = m_functor();
			m_q_ptr->put(forward<T>(val.first), val.second);
		}
		log_debug ("WRITER IS OVER");
	}
private:
	inline void on_start(){ on_event(EV_START);}
	inline void on_stop() { on_event(EV_STOP); }
	inline void on_hwm()  { on_event(EV_HWM);  }
	inline void on_lwm()  { on_event(EV_LWM);  }
	
	enum WriterState {
		STOPPED  = 0,
		SENDING  = 1,
		SLEEPING = 2,
		COUNT_STATES
	};
	enum WriterEvents {
		EV_START = 0,
		EV_STOP  = 1,
		EV_HWM   = 2,
		EV_LWM   = 3,
		COUNT_EVENTS
	};
	WriterState m_current_state = STOPPED;
	void(Writer::*m_trans_table[COUNT_STATES][COUNT_EVENTS])(void)  =
	{   //EV_START				EV_STOP				EV_HWM				EV_LWM
		{ &Writer::do_start,	nullptr,			nullptr,			nullptr},            //STOPPED
	    { nullptr,				&Writer::do_stop,	&Writer::do_hwm,	nullptr },           //SENDING
	    { nullptr,				&Writer::do_stop,	nullptr,			&Writer::do_lwm } }; //SLEEPING
	
	inline void on_event(WriterEvents e) {
		auto action = m_trans_table[m_current_state][e];
		if (action != nullptr) (*this.*action)();
	}
	
	inline void do_start() {
		log_debug("WRITER DO_START");
		m_current_state = SENDING;
	}
	inline void do_stop() {
		log_debug("WRITER DO_STOP");
		m_current_state = STOPPED;
		m_sleeping_cond.notify_one();
	}
	inline void do_hwm() {
		log_verbose("WRITER DO_HWM");
		m_current_state = SLEEPING;
	}
	inline void do_lwm() {
		log_verbose("WRITER DO_LWM");
		m_current_state = SENDING;
		m_sleeping_cond.notify_one();
	}
};

