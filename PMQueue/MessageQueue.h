#pragma once
#include "RetCodes.h"
#include <mutex>
#include <list>
#include <queue>
#include <condition_variable>
#include "IMessageQueueEvents.h"
#include "Log.h"
using uint = unsigned int;
template <typename T >
class MessageQueue
{
public:

	inline RetCodes get(T& item) {
		std::unique_lock<std::mutex> mlock(m_mutex);
		while (m_queue_ptr->empty() && m_is_started)
		{
			m_non_empty_cond.wait(mlock);
		}
		if (!m_is_started) return RetCodes::STOPED;
		item = std::move(const_cast<T&>(m_queue_ptr->top().first));
		m_queue_ptr->pop();
		log_verbose("QUEUE READ");
		lwm_check();
		mlock.unlock();
		return RetCodes::OK;
	}
	inline RetCodes put(T&& item, int priority)
	{
		return put_(move(item), priority);
	}
	inline RetCodes put(const T& item, int priority)
	{
		return put_(T(item), priority);
	}

	void start() {
		std::unique_lock<std::mutex> mlock(m_mutex);
		if (m_is_started) return;
		m_is_started = true;
		notify_start();
	}
	void stop() {
		std::unique_lock<std::mutex> mlock(m_mutex);
		if (!m_is_started) return;
		m_is_started = false;
		notify_stop();
		mlock.unlock();
		m_non_empty_cond.notify_all();
	}
	void set_events(IMessageQueueEvents* subscriber)
	{
		std::unique_lock<std::mutex> mlock(m_mutex);
	    m_handlers.push_back(subscriber);
	}

	MessageQueue(uint queue_size, uint hwm, uint lwm) :
		m_queue_size{ max(1u,queue_size) },
		m_hwm{ max(1u,min(hwm,queue_size)) },
		m_lwm{ max(0u, min(queue_size,min(hwm,lwm))) }
	{
		vector<P> vec;
		vec.reserve(m_queue_size);
		m_queue_ptr = std::make_unique<std::priority_queue < P, vector<P>, Compare>>(Compare(), move(vec));

	}
	MessageQueue(const MessageQueue&) = delete;
	MessageQueue(MessageQueue&&) = delete;
	
	~MessageQueue() {
		try {
			stop();
		}
		catch (...) {
			//destructor fails
		}
	}

private:
	inline RetCodes put_(T&& item, int priority)
	{
		std::unique_lock<std::mutex> mlock(m_mutex);
		if (!m_is_started) return RetCodes::STOPED;
		if (hwl_check() || m_shedding_is_on) return RetCodes::HWM;
		try {
			m_queue_ptr->push(make_pair(move(item), priority));
		}
		catch (...) {
			return RetCodes::NO_SPACE;
		}
		log_verbose("QUEUE WRITE");
		mlock.unlock();
		m_non_empty_cond.notify_one();
		return RetCodes::OK;
	}
	using P = pair<T, int>;
	struct Compare {
		bool operator()(P const & p1, P const & p2) {
			return p1.second > p2.second;
		}
	};
	inline bool hwl_check() {
		auto res = (m_queue_ptr->size() >= m_hwm);
		if (res && !m_shedding_is_on) {
			notify_hwm();
			m_shedding_is_on = true;
		}
		return res;
	}

	inline bool lwm_check() {
		auto res =  (m_queue_ptr->size() <= m_lwm);
		if (res && m_shedding_is_on) {
			notify_lwm();
			m_shedding_is_on = false;
		}
		return res;
	}
	inline void notify_lwm() {
		log_verbose("SHEDDING IS OFF");
		for (const auto& e : m_handlers) {
			if(e != nullptr) e->on_lwm();
		}
	}
	inline void notify_hwm() {
		log_verbose("SHEDDING IS ON");
		for (const auto& e : m_handlers) {
			if (e != nullptr) e->on_hwm();
		}
	}
	inline void notify_start() {
		log_debug("QUEUE is STARTED");
		for (const auto& e : m_handlers) {
			if (e != nullptr) e->on_start();
		}
	}
	inline void notify_stop() {
		log_debug("QUEUE is STOPPED");
		for (const auto& e : m_handlers) {
			if (e != nullptr) e->on_stop();
		}
	}

	uint m_queue_size;
	uint m_hwm;
	uint m_lwm;
	bool m_shedding_is_on = false;
	bool m_is_started = false;
	unique_ptr<std::priority_queue < P, vector<P>, Compare>> m_queue_ptr;
	std::mutex m_mutex;
	std::condition_variable m_non_empty_cond;
	using HandlersList = std::list<IMessageQueueEvents*>;
	HandlersList m_handlers;
};

