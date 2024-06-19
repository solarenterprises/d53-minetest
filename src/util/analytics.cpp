#include "analytics.h"
#include "convert_json.h"
#include "settings.h"
#include "threading/mutex_auto_lock.h"

Analytics g_analytics;

const std::string Analytics::getThreadName()
{
	std::thread::id id = std::this_thread::get_id();

	{
		MutexAutoLock lock(m_mutex);
		std::map<std::thread::id, std::string>::const_iterator it = m_thread_names.find(id);
		if (it != m_thread_names.end())
			return it->second;
	}

	std::ostringstream os;
	os << "#0x" << std::hex << id;
	return os.str();
}

void Analytics::registerThread(const std::string& name)
{
	std::thread::id id = std::this_thread::get_id();
	MutexAutoLock lock(m_mutex);
	m_thread_names[id] = name;
}

void Analytics::deregisterThread()
{
	std::thread::id id = std::this_thread::get_id();
	MutexAutoLock lock(m_mutex);
	m_thread_names.erase(id);
}

//void Analytics::log_error_stream(std::string error_str) {
//	std::thread::id id = std::this_thread::get_id();
//	MutexAutoLock lock(m_log_error_stream_mutex[id]);
//
//	size_t index_end = error_str.find('\n');
//	if (index_end != std::string::npos) {
//		std::string err = m_log_error_stream[id] + error_str.substr(0, index_end);
//		if (index_end+1 >= error_str.size())
//			m_log_error_stream[id] = "";
//		else
//			m_log_error_stream[id] = error_str.substr(index_end+1);
//
//		std::string thread_name;
//		{
//			MutexAutoLock lock(m_mutex);
//			std::map<std::thread::id, std::string>::const_iterator it_name = m_thread_names.find(id);
//			if (it_name != m_thread_names.end())
//				thread_name = it_name->second;
//		}
//
//		post(thread_name, project_name, "error", err, json_null);
//	}
//
//	if (error_str.empty())
//		return;
//
//	m_log_error_stream[id] += error_str;
//}

void Analytics::handle_http_requests() {
	for (int i = 0; i < http_requests.size(); i++) {
		auto& http_id = http_requests[i];

		HTTPFetchResult result;
		if (!httpfetch_async_get(http_id, result))
			continue;

		httpfetch_caller_free(http_id);

		http_requests.erase(http_requests.begin() + i);
		i--;
	}

	//
	// Print errors
	/*for (auto& it : m_log_error_stream_mutex) {
		std::string str;
		std::string thread_name;
		{
			MutexAutoLock lock(it.second);
			str = m_log_error_stream[it.first];
			m_log_error_stream[it.first] = "";
		}

		if (str.empty())
			continue;

		{
			MutexAutoLock lock(m_mutex);
			std::map<std::thread::id, std::string>::const_iterator it_name = m_thread_names.find(it.first);
			if (it_name != m_thread_names.end())
				thread_name = it_name->second;
		}

		post(thread_name, project_name, "error", str, json_null);
	}*/
}

void Analytics::post(std::string thread_name, std::string project_name, std::string type, std::string text, const Json::Value& json) {
	if (!g_settings->getBool("analytics_enabled"))
		return;

	Json::Value j;
	j["type"] = type;
	j["text"] = text;
	j["user"] = m_thread_user[std::this_thread::get_id()];

	if (thread_name.empty())
		thread_name = getThreadName();

	j["project"] = "["+ thread_name + "]" + project_name;
	j["data"] = fastWriteJson(json);

	const std::string api_key = g_settings->get("analytics_api_key");
	
	HTTPFetchRequest request;
	request.method = HTTP_POST;
	request.extra_headers.emplace_back("Content-Type: application/json");

	request.url = g_settings->get("analytics_url");
	if (!api_key.empty()) {
		request.url += "/admin";
		request.extra_headers.emplace_back("authorization=" + api_key);
	}
	request.url += "/entry";

	request.caller = httpfetch_caller_alloc();
	request.raw_data = fastWriteJson(j);

	http_requests.push_back(request.caller);
	httpfetch_async(request);
}
