#include <algorithm>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <unordered_map>

//--------------------------------------------------------------------------------------------------
// Parse one request
//--------------------------------------------------------------------------------------------------
bool parseRequest (const std::string& request,
									 std::string& host,
									 std::tm& time,
									 std::string& resource,
									 int& size)
{
	//------------------------------------------------------------------------------------------------
	// Parse the hostname
	//------------------------------------------------------------------------------------------------
	size_t pos = request.find(" ", 0);
	host = request.substr(0, pos);

	//------------------------------------------------------------------------------------------------
	// Parse the time
	//------------------------------------------------------------------------------------------------
	pos = request.find("[", pos) + 1;
	std::stringstream ss(request.substr(pos, request.find("]", pos) - pos));
	ss.imbue(std::locale(setlocale(LC_ALL, nullptr)));
	ss >> std::get_time(&time, "%d/%b/%Y:%T %z");

	//------------------------------------------------------------------------------------------------
	// Parse the command (ignore command and protocol)
	//------------------------------------------------------------------------------------------------
	pos = request.find("\"", pos) + 1;
	pos = request.find(" ", pos) + 1;
	resource = request.substr(pos, request.find(" ", pos) - pos);
	pos = request.find_last_of("\"", std::string::npos) + 1;
		
	//------------------------------------------------------------------------------------------------
	// Parse the return code
	//------------------------------------------------------------------------------------------------
	pos = request.find_first_not_of(" ", pos);
	int code;
	try {
		code = stoi(request.substr(pos, request.find(" ", pos) - pos));
	} catch (const std::invalid_argument& ia) {
		std::cerr << request << std::endl;
		std::cerr << request.substr(pos, request.find(" ", pos) - pos) << std::endl;
		exit(1);
	}
	
	//------------------------------------------------------------------------------------------------
	// Parse the resource size
	//------------------------------------------------------------------------------------------------
	pos = request.find_first_not_of(" ", request.find(" ", pos) + 1);
	try {
		size = (request[pos] == '-') ? 0 : stoi(request.substr(pos, std::string::npos));
	} catch (const std::invalid_argument& ia) {
		std::cerr << "Invalid argument: " << ia.what() << std::endl;
		exit(1);
	}

	return (code != 401);
}

template<typename T1, typename T2>
void updateList (std::vector<std::pair<T1, T2>>& list,
								 int& size,
								 const int& max_size,
								 const T1& key,
								 T2& val,
								 T2& min_val)
{
	//----------------------------------------------------------------------------------------------
	// Try to find if this key is on the list, the old one is removed
	//----------------------------------------------------------------------------------------------
	int pos = std::find_if(list.begin(), list.begin() + size,
			[&] (const std::pair<T1, T2>& h) {
			return h.first == key;
			}) - list.begin();
	//----------------------------------------------------------------------------------------------
	// If this key is not on the list but the list is not full, add one host 
	//----------------------------------------------------------------------------------------------
	if (pos == size && size < max_size) ++size;
	//----------------------------------------------------------------------------------------------
	// If the list is full but the number of accesses is greater than the min, remove the last one
	//----------------------------------------------------------------------------------------------
	if (pos == max_size && val > min_val) pos = max_size - 1;
	if (pos < max_size) {
		//--------------------------------------------------------------------------------------------
		// Move the keys with fewer accesses down the list until the right position is found,
		// then insert the key to the list
		//--------------------------------------------------------------------------------------------
		for (int i = pos; i >= 0; --i) {
			if (i > 0 && list[i - 1].second < val) {
				list[i] = list[i - 1];
			} else {
				list[i] = std::make_pair(key, val);
				break;
			}
		}
		//--------------------------------------------------------------------------------------------
		// Update the minimum value of the list 
		//--------------------------------------------------------------------------------------------
		min_val = list[size - 1].second;
	}
}

bool processLogFile (const std::string& filename,
									 std::vector<std::pair<std::string, int>>& top_hosts,
									 std::vector<std::pair<std::string, int>>& top_resources,
									 std::vector<std::pair<std::tm, int>>& busiest_hours,
									 std::vector<std::string>& blocked)
{
	std::ifstream logFile(filename.c_str());
	if (!logFile.good()) {
		std::cerr << "Error when reading " << filename << std::endl;
		return false;
	}

	bool success;
	std::string request,
							host,
							resource;
	std::tm prev_time,
					curr_time;
	time_t prev_seconds = 0,
				 curr_seconds = 0,
				 global_seconds = 0;
	int size = 0,
			pos = 0,
			elapsed_time = 0,
			diff_seconds = 0,
			min_accesses = 0,
			min_bandwidth = 0,
			min_hour_freq = 0,
			num_hosts = 0,
			num_accesses = 0,
			num_resources = 0,
			bandwidth = 0,
			num_hours = 0,
			hour_freq = 0,
			prev_second_of_hour = 0,
			curr_second_of_hour = 0;
	std::unordered_map<std::string, int> host_accesses,
																			 resource_bandwidth;
	std::unordered_map<std::string, int>::iterator prev_access,
																								 prev_resource;
	std::unordered_map<std::string, std::pair<int, time_t>> failed_attempts;
	std::unordered_map<std::string, std::pair<int, time_t>>::iterator prev_attempt;
	std::vector<int> freq_per_second(3600, 0);

	while (std::getline(logFile, request)) {
		success = parseRequest(request, host, curr_time, resource, size);
		curr_seconds = mktime(&curr_time);
		if (global_seconds == 0) global_seconds = curr_seconds;
		
		//----------------------------------------------------------------------------------------------
		// Feature 1: tracking top accessing hosts
		//----------------------------------------------------------------------------------------------
		prev_access = host_accesses.find(host);
		if (prev_access == host_accesses.end()) {
			//--------------------------------------------------------------------------------------------
			// If there is no record of this host, create a new one with 1 access 
			//--------------------------------------------------------------------------------------------
			host_accesses[host] = 1;
			num_accesses = 1;
		} else {
			//--------------------------------------------------------------------------------------------
			// If there is a record of this host, increment the number of accesses 
			//--------------------------------------------------------------------------------------------
			++prev_access->second;
			num_accesses = prev_access->second;
		}
		//----------------------------------------------------------------------------------------------
		// Update the top list of hosts 
		//----------------------------------------------------------------------------------------------
		updateList(top_hosts, num_hosts, 10, host, num_accesses, min_accesses);
		
		//----------------------------------------------------------------------------------------------
		// Feature 2: tracking top accessed resources bandwidth
		//----------------------------------------------------------------------------------------------
		prev_resource = resource_bandwidth.find(resource);
		if (prev_resource == resource_bandwidth.end()) {
			//--------------------------------------------------------------------------------------------
			// If there is no record of this resource, create a new one with the size as the bandwidth
			//--------------------------------------------------------------------------------------------
			resource_bandwidth[resource] = size;
			bandwidth = size;
		} else {
			//--------------------------------------------------------------------------------------------
			// If there is a record of this resource, add the size to the bandwidth 
			//--------------------------------------------------------------------------------------------
			prev_resource->second += size;
			bandwidth = prev_resource->second;
		}
		//----------------------------------------------------------------------------------------------
		// Update the top list of resources 
		//----------------------------------------------------------------------------------------------
		updateList(top_resources, num_resources, 10, resource, bandwidth, min_bandwidth);
		
		//----------------------------------------------------------------------------------------------
		// Feature 3: tracking busiest hours
		//----------------------------------------------------------------------------------------------
		if (prev_seconds == 0) {
			//--------------------------------------------------------------------------------------------
			// Initialize the start of the window
			//--------------------------------------------------------------------------------------------
			prev_seconds = curr_seconds;
			prev_time = curr_time;
		}
		//----------------------------------------------------------------------------------------------
		// Compute the next window size and corresponding slots
		//----------------------------------------------------------------------------------------------
		diff_seconds = difftime(curr_seconds, prev_seconds);
		curr_second_of_hour = curr_time.tm_min * 60 + curr_time.tm_sec;
		prev_second_of_hour = prev_time.tm_min * 60 + prev_time.tm_sec;
		while (diff_seconds >= 3600) {
			//--------------------------------------------------------------------------------------------
			// If the next window size is over an hour, update the list before expanding the window
			//--------------------------------------------------------------------------------------------
			if (num_hours < 10 || hour_freq > min_hour_freq) {
				if (num_hours < 10) ++num_hours;
				for (int i = num_hours - 1; i >= 0; --i) {
					if (i > 0 && busiest_hours[i - 1].second < hour_freq) {
						busiest_hours[i] = busiest_hours[i - 1];
					} else {
						busiest_hours[i] = std::make_pair(prev_time, hour_freq);
						break;
					}
				}
				min_hour_freq = busiest_hours[num_hours - 1].second;
			}
			//--------------------------------------------------------------------------------------------
			// Shrink the window by moving the starting slot and decrease the total frequency 
			//--------------------------------------------------------------------------------------------
			hour_freq -= freq_per_second[prev_second_of_hour];
			freq_per_second[prev_second_of_hour] = 0;
			if (++prev_second_of_hour >= 3600) prev_second_of_hour = 0;
			--diff_seconds;
			++prev_seconds;
			prev_time = curr_time;
			prev_time.tm_min -= diff_seconds / 60;
			prev_time.tm_sec -= diff_seconds % 60;
			mktime(&prev_time);
		}
		//----------------------------------------------------------------------------------------------
		// Update the slot and total frequencies
		//----------------------------------------------------------------------------------------------
		++freq_per_second[curr_second_of_hour];
		++hour_freq;

		//----------------------------------------------------------------------------------------------
		// Feature 4: blocking login for 5 minutes after 3 failed login attempts within 20 seconds
		//----------------------------------------------------------------------------------------------
		prev_attempt = failed_attempts.find(host);
		if (prev_attempt == failed_attempts.end()) {
			if (!success) {
				//------------------------------------------------------------------------------------------
				// If login fails and no previous failure record, add the host and time to the list
				//------------------------------------------------------------------------------------------
				failed_attempts[host] = std::make_pair(1, curr_seconds);
			}
		} else {
			elapsed_time = std::difftime(curr_seconds, prev_attempt->second.second);
			int& failures = prev_attempt->second.first;
			if (failures >= 3 && elapsed_time <= 300) {
				//------------------------------------------------------------------------------------------
				// If there were 3 failed login attempts within 5 minutes, log this request
				//------------------------------------------------------------------------------------------
				blocked.push_back(request);
			} else if (!success) {
				if (failures < 3 && elapsed_time <= 20) {
					//----------------------------------------------------------------------------------------
					// If the login fails and it is within 20 seconds window, increment the number of failures
					//----------------------------------------------------------------------------------------
					++failures;
				} else {
					//----------------------------------------------------------------------------------------
					// If the login fails and it is outside 20 seconds window, reset the record
					//----------------------------------------------------------------------------------------
					prev_attempt->second = std::make_pair(1, curr_seconds);
				}
			} else {
				//------------------------------------------------------------------------------------------
				// If the login succeeds, remove the host from the list
				//------------------------------------------------------------------------------------------
				failed_attempts.erase(prev_attempt);
			}
		}
		if (std::difftime(curr_seconds, global_seconds) > 1200) {
			//--------------------------------------------------------------------------------------------
			// Clean up expired failures every 20 minutes
			//--------------------------------------------------------------------------------------------
			global_seconds = curr_seconds;
			for (auto fa = failed_attempts.begin(); fa != failed_attempts.end(); ) {
				elapsed_time = std::difftime(curr_seconds, fa->second.second);
				int failures = fa->second.first;
				if ((failures < 3 && elapsed_time > 20) || (failures >= 3 && elapsed_time > 300)) {
					fa = failed_attempts.erase(fa);
				} else {
					++fa;
				}
			}
		}
	}

	logFile.close();

	//------------------------------------------------------------------------------------------------
	// Feature 3: tracking busiest hours for intervals less than an hour
	//------------------------------------------------------------------------------------------------
	if (prev_seconds != 0) {
		prev_second_of_hour = prev_time.tm_min * 60 + prev_time.tm_sec;
		int t = prev_second_of_hour;
		const int et = (t + 3599) % 3600;
		while (t != et) {
			pos = num_hours;
			if (num_hours < 10) {
				++num_hours;
			} else if (hour_freq > min_hour_freq) {
				pos = num_hours - 1;
			}
			if (pos < 10) {
				for (int i = pos; i >= 0; --i) {
					if (i > 0 && busiest_hours[i - 1].second < hour_freq) {
						busiest_hours[i] = busiest_hours[i - 1];
					} else {
						busiest_hours[i] = std::make_pair(prev_time, hour_freq);
						break;
					}
				}
				min_hour_freq = busiest_hours[num_hours - 1].second;
			}
			hour_freq -= freq_per_second[t];
			if (num_hours == 10 && hour_freq < min_hour_freq) break;
			freq_per_second[t] = 0;
			++prev_time.tm_sec;
			mktime(&prev_time);
			if (++t >= 3600) t = 0;
		}
	}

	//------------------------------------------------------------------------------------------------
	// Resize the lists 
	//------------------------------------------------------------------------------------------------
	top_hosts.resize(num_hosts);
	top_resources.resize(num_resources);
	busiest_hours.resize(num_hours);

	return true;
}

int main (int argc,
					char* argv[])
{
	if (argc != 6) {
		std::cerr << "Usage: " << argv[0] << " <log_input> <hosts> <resources> <hours> <blocked>"
			<< std::endl;
		return 1;
	}

	std::vector<std::pair<std::string, int>> top_hosts(10),
																					 top_resources(10);
	std::vector<std::pair<std::tm, int>> busiest_hours(10);
	std::vector<std::string> blocked;
	
	if (processLogFile(std::string(argv[1]), top_hosts, top_resources, busiest_hours, blocked)) {
		//----------------------------------------------------------------------------------------------
		// Write to the top host file
		//----------------------------------------------------------------------------------------------
		std::ofstream hostsFile(argv[2]);
		if (hostsFile.good()) {
			for (auto& host : top_hosts) {
				hostsFile << host.first << "," << host.second << std::endl;
			}
			hostsFile.close();
		}
		
		//----------------------------------------------------------------------------------------------
		// Write to the top resources file
		//----------------------------------------------------------------------------------------------
		std::ofstream resourcesFile(argv[3]);
		if (resourcesFile.good()) {
			for (auto& resource : top_resources) {
				resourcesFile << resource.first << std::endl;
			}
			resourcesFile.close();
		}
		
		//----------------------------------------------------------------------------------------------
		// Write to the busiest hours file
		//----------------------------------------------------------------------------------------------
		std::ofstream hoursFile(argv[4]);
		if (hoursFile.good()) {
			for (auto& hour : busiest_hours) {
				hoursFile << std::put_time(&hour.first, "%d/%b/%Y:%T") << " -0400," << hour.second
									<< std::endl;
			}
			hoursFile.close();
		}
		
		//----------------------------------------------------------------------------------------------
		// Write to the blocked list file
		//----------------------------------------------------------------------------------------------
		std::ofstream blockedFile(argv[5]);
		if (blockedFile.good()) {
			for (auto& request : blocked) {
				blockedFile << request << std::endl;
			}
			blockedFile.close();
		}
	}

	return 0;
}
