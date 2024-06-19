#pragma once

#include <string>
#include <json/json.h>
#include <thread>
#include <mutex>
#include "httpfetch.h"

class Analytics {
public:
	Json::Value json_null;

	void registerThread(const std::string& name);
	void deregisterThread();

	void set_user(std::string user) {
		std::thread::id id = std::this_thread::get_id();
		this->m_thread_user[id] = user;
	}

	void set_project_name(std::string project_name) {
		this->project_name = project_name;
	}

	//void log_error_stream(std::string error_str);

	void log_error(std::string thread_name, std::string error_str) {
		post(thread_name, project_name, "error", error_str, json_null);
	}

	void log_event(std::string event_name, const Json::Value& json) {
		post("", project_name, "event", event_name, json);
	}

	void handle_http_requests();

private:
	void post(std::string thread_name, std::string project_name, std::string type, std::string text, const Json::Value& json);
	const std::string getThreadName();

	std::string project_name = "unknown";

	/*
	* HTTP
	*/
	std::vector<u64> http_requests;

	//std::map<std::thread::id, std::mutex> m_log_error_stream_mutex;
	//std::map<std::thread::id, std::string> m_log_error_stream;

	std::map<std::thread::id, std::string> m_thread_names;
	std::map<std::thread::id, std::string> m_thread_user;
	mutable std::mutex m_mutex;
};

extern Analytics g_analytics;
