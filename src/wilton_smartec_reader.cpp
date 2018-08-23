#include <iostream>
#include <thread>
#include <cstring>
#include <string>

#ifdef WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/select.h>
#include <poll.h>
#include <math.h>
#endif


#include "wilton/wiltoncall.h"


namespace {
    timeval get_time_val(int milleseconds){
        int seconds = milleseconds / 1000; // div
        int microseconds = (milleseconds % 1000)*1000; // mod
        return timeval{seconds, microseconds};
    }
} // namespace

namespace example {

std::string hello(const std::string& input) {
    // arbitrary C++ code here
    return input + " from C++!";
}

std::string hello_again(const std::string& input) {
    // arbitrary C++ code here
    return input + " again from C++!";
}


std::string read_smartec_input(int timeout) {
    std::string data{""};

    std::thread input([&data, &timeout](){
#ifdef WIN32
        HANDLE event_handle = GetStdHandle(STD_INPUT_HANDLE);

        const DWORD timeout = timeout;
        DWORD result = WaitForSingleObject(
          event_handle,
          timeout
        );
        switch(result) {
        case WAIT_OBJECT_0: {
            std::cin >> data;
            break;
        }
        case WAIT_TIMEOUT: {break;}
        case WAIT_ABANDONED: {break;}
        case WAIT_FAILED: {break;}
        }
#else
            //Below cin operation should be executed within stipulated period of time
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(STDIN_FILENO, &readSet);
        struct timeval tv = get_time_val(timeout);
        if (select(STDIN_FILENO+1, &readSet, NULL, NULL, &tv) < 0) std::perror("select");

        if (FD_ISSET(STDIN_FILENO, &readSet)) {
            std::cin >> data;
        }
#endif
    });

    if (input.joinable()) {
        input.join();
    }
    return data;
}

// helper function
char* wrapper_read_smartec_input(void* ctx, const char* data_in, int data_in_len, char** data_out, int* data_out_len) {
    try {
        auto fun = reinterpret_cast<std::string(*)(int)> (ctx);
        auto input = std::string(data_in, static_cast<size_t>(data_in_len));
        auto timeout = std::stoi(input);
        std::string output = fun(timeout);
        if (!output.empty()) {
            // nul termination here is required only for JavaScriptCore engine
            *data_out = wilton_alloc(static_cast<int>(output.length()) + 1);
            std::memcpy(*data_out, output.c_str(), output.length() + 1);
        } else {
            *data_out = nullptr;
        }
        *data_out_len = static_cast<int>(output.length());
        return nullptr;
    } catch (...) {
        auto what = std::string("CALL ERROR"); // std::string(e.what());
        char* err = wilton_alloc(static_cast<int>(what.length()) + 1);
        std::memcpy(err, what.c_str(), what.length() + 1);
        return err;
    }
}


// helper function
char* wrapper_fun(void* ctx, const char* data_in, int data_in_len, char** data_out, int* data_out_len) {
    try {
        auto fun = reinterpret_cast<std::string(*)(const std::string&)> (ctx);
        auto input = std::string(data_in, static_cast<size_t>(data_in_len));
        std::string output = fun(input);
        if (!output.empty()) {
            // nul termination here is required only for JavaScriptCore engine
            *data_out = wilton_alloc(static_cast<int>(output.length()) + 1);
            std::memcpy(*data_out, output.c_str(), output.length() + 1);
        } else {
            *data_out = nullptr;
        }
        *data_out_len = static_cast<int>(output.length());
        return nullptr;
    } catch (...) {
        auto what = std::string("CALL ERROR"); // std::string(e.what());
        char* err = wilton_alloc(static_cast<int>(what.length()) + 1);
        std::memcpy(err, what.c_str(), what.length() + 1);
        return err;
    }
}

} // namespace

// this function is called on module load,
// must return NULL on success
extern "C"
#ifdef _WIN32
__declspec(dllexport)
#endif
char* wilton_module_init() {
    char* err = nullptr;


    // register 'hello' function
    auto read_smartec_input_name = std::string("read_smartec_input");
    err = wiltoncall_register(read_smartec_input_name.c_str(), static_cast<int> (read_smartec_input_name.length()),
            reinterpret_cast<void*> (example::read_smartec_input), example::wrapper_read_smartec_input);
    if (nullptr != err) return err;

    // register 'hello' function
    auto name_hello = std::string("example_hello");
    err = wiltoncall_register(name_hello.c_str(), static_cast<int> (name_hello.length()), 
            reinterpret_cast<void*> (example::hello), example::wrapper_fun);
    if (nullptr != err) return err;

    // register 'hello_again' function
    auto name_hello_again = std::string("example_hello_again");
    err = wiltoncall_register(name_hello_again.c_str(), static_cast<int> (name_hello_again.length()), 
            reinterpret_cast<void*> (example::hello_again), example::wrapper_fun);
    if (nullptr != err) return err;

    // return success
    return nullptr;
}
