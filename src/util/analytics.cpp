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
		auto it = m_thread_names.find(id);
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

void Analytics::handle_http_requests() {
	//
	// Take requests into a temp vector
	std::vector<u64> requests;
	{
		MutexAutoLock lock(m_mutex_requests);
		requests = http_requests;
		http_requests.clear();
	}

	const long TooManyRequests = 429;

	for (int i = 0; i < requests.size(); i++) {
		auto& http_id = requests[i];

		HTTPFetchResult result;
		if (!httpfetch_async_get(http_id, result))
			continue;

		if (result.response_code == TooManyRequests) {
			time_too_many_requests = std::time(nullptr)+60;
		}

		httpfetch_caller_free_async(http_id);

		requests.erase(requests.begin() + i);
		i--;
	}

	//
	// Add back requests
	{
		MutexAutoLock lock(m_mutex_requests);
		http_requests.insert(http_requests.begin(), requests.begin(), requests.end());
	}
}

void Analytics::post(std::string thread_name, std::string project_name, std::string type, std::string text, const Json::Value& json) {
	if (!is_enabled)
		return;

	if (std::difftime(std::time(nullptr), time_too_many_requests) < 0)
		return;

	Json::Value j;
	j["type"] = type;
	j["text"] = text;
	j["user"] = m_thread_user[std::this_thread::get_id()];

	if (thread_name.empty())
		thread_name = getThreadName();

	j["project"] = game_name + " ["+ thread_name + "]" + project_name;
	j["data"] = fastWriteJson(json);

	const std::string api_key = g_settings->get("analytics_api_key");
	
	HTTPFetchRequest request;
	request.auto_print_error = false;
	request.method = HTTP_POST;
	request.extra_headers.emplace_back("Content-Type: application/json");

	request.url = g_settings->get("analytics_url");
	if (!api_key.empty()) {
		request.url += "/admin";
		request.extra_headers.emplace_back("authorization=" + api_key);
	}
	request.url += "/entry";

	
	request.raw_data = fastWriteJson(j);

	{
		MutexAutoLock lock(m_mutex_requests);
		request.caller = httpfetch_caller_alloc();
		http_requests.push_back(request.caller);
	}
	httpfetch_async(request);
}
