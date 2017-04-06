# Table of Contents
1. [Required Library](README.md#library)
2. [Details of Implementation](README.md#details-of-implementation)


# Required Library
The whole program is written in C++11 and only relies on the standard C++ library. However, the `get_time` and `put_time` functions are not included in GCC 4.9. GCC 5 or above is needed. Other compilers may also be used. A `makefile` is provided in the `src` directory.

## Details of Implementation
The program is commented within the source file. Here are the details of the implementation for each features.

### Feature 1 
List in descending order the top 10 most active hosts/IP addresses that have accessed the site.

This feature is implemented using 2 data structures: a hashmap (`unordered_map`) and an array (`vector`). The first one is used for fast searching for the hosts and their number of accesses. A hashmap is the most efficient data structure for searching. The second one is used for a sorted list of the most active hosts/IP addresses. Since the size of the sorted list is small, an array is used instead. A simple insertion sort algorithm is implemented for efficienct sorting for small array. The size of the array is initialized to be 10 but a true size is kept in a separate variable. This is for minimizing the memory allocation time and better implementation of the insertion sort algorithm.

### Feature 2 
Identify the top 10 resources on the site that consume the most bandwidth. Bandwidth consumption can be extrapolated from bytes sent over the network and the frequency by which they were accessed.

Similar implementation to Feature 1 is used except the used bandwidth is computed by incrementing the size of the resource requested every time.

### Feature 3 
List in descending order the site’s 10 busiest (i.e. most frequently visited) 60-minute period.

An array of 3600 integers is used for the 60-minute window (60 x 60 seconds). The request time is translated to an index within the window. For example, an incoming request at 07:03:07 is translated to 3 * 60 + 7 = 187 and the 187th integer is used to store the access frequency at that time. The beginning index of the window is stored. If the duration between the current time and the beginning of the window is larger than 60 minutes, the total frquency of the existing window, which is stored and updated for every incoming request, is now associated with the time of the beginning of the window and stored in the list of 10 busiest 60-minute period if it is higher than any of that on the list. The beginning of the window is then moved to the right and the previous frequency is erased until the window size is under 60 minutes.

After all requests are processed, the 60-minute window is processed for window size less than 60-minute and the list of the 10 busiest 60-minute period is updated accordingly.

### Feature 4 
Your final task is to detect patterns of three consecutive failed login attempts over 20 seconds in order to block all further attempts to reach the site from the same IP address for the next 5 minutes. Each attempt that would have been blocked should be written to a log file named `blocked.txt`.

This feature is implemented using a hashmap to store the hosts that fail to login. There are 2 integers other than the host name. The first one is the number of failed attempts and the second one is the elapsed time since the first failed login in case the number of failed attempts is less than 3, or the elapsed time since the third failed attempts within 20 seconds. The hashmap is used for efficient searching for the hosts. The list is updated when the same host sends a request. If the login attempt is within the blocked period of 5 minutes, the request is logged in an array.

The hashmap of the failed attempts of the hosts is cleaned up every 20 minutes according to the access time of the current request and the last clean up time. All expired host records are erased for more efficient searching.
