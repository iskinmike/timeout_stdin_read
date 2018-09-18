
#include <thread>
#include <string>
#include <cstring>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <atomic>

#ifdef WIN32
#include <windows.h>
#else
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/X.h>

#include <set>
#endif

#include "wilton/wiltoncall.h"

#ifdef WIN32
class key_logger
{
    std::mutex cond_mtx;
    std::condition_variable cond;

    DWORD thread_id;

    HHOOK hook;
    std::thread timer_thread;
    std::cv_status status;
public:
    std::atomic_bool data_recieved;
    std::string data;
    key_logger() : data(""), hook(nullptr), thread_id(NULL), status(std::cv_status::no_timeout) {
        data_recieved.exchange(false);
    }
    ~key_logger(){
        stop_logger();
    };

    static key_logger& get_instance()
    {
        static key_logger logger;
        return logger;
    }

    bool wait_result_or_timeout(int timeout) {
        MSG message;
        thread_id = GetCurrentThreadId();
        timer_thread = std::thread([this, timeout](){
            std::unique_lock<std::mutex> lock(this->cond_mtx);
            while (!this->data_recieved) {
                this->status = this->cond.wait_for(lock, std::chrono::milliseconds(timeout));
                if (std::cv_status::timeout == this->status) {
                    break;
                }
            }
            get_instance().stop_logger();
        });
        GetMessage(&message, NULL, 0, 0);
        TranslateMessage(&message);
        DispatchMessage(&message);
        if (timer_thread.joinable()) timer_thread.join();
        return data_recieved;
    }

    bool is_timer_expired(){
        return (std::cv_status::timeout == status);
    }
    std::string get_data_as_json(){
        std::string json("{ \"data\": \"");
        json += data;
        json += "\", \"is_expired\": ";
        json += (is_timer_expired()) ? "true" : "false";
        json += "}";
        return json;
    }

    std::string init_logger(){
        data.clear();
        hook = SetWindowsHookEx(
            WH_KEYBOARD_LL,
            static_cast<HOOKPROC>(&key_logger::keyboard_hook_proc),
            GetModuleHandle(NULL), // current HInstance
            0);
        if (hook == nullptr) {
            return std::string("{ \"error\": \"Init keyboard hook error: " + std::to_string(GetLastError()) + "\"}");
        }
        return std::string{};
    }

    void stop_logger() {
        if (nullptr != hook) {
            UnhookWindowsHookEx(hook);
            PostThreadMessage(thread_id, NULL, 0, 0);
            cond.notify_all();
        }
    }

    static LRESULT CALLBACK keyboard_hook_proc(int nCode, WPARAM wParam, LPARAM lParam) {
        KBDLLHOOKSTRUCT *shortcut = (KBDLLHOOKSTRUCT *)lParam;
        if (nCode == HC_ACTION){
            // Start logging keys
            if (wParam == WM_SYSKEYDOWN || wParam == WM_KEYDOWN) // If key has been pressed
            {
                // Compare virtual keycode to hex values and log keys accordingly
                switch (shortcut->vkCode)  {
                //Number keys
                case 0x60:
                case 0x30: key_logger::get_instance().data.append("0"); break;
                case 0x61:
                case 0x31: key_logger::get_instance().data.append("1"); break;
                case 0x62:
                case 0x32: key_logger::get_instance().data.append("2"); break;
                case 0x63:
                case 0x33: key_logger::get_instance().data.append("3"); break;
                case 0x64:
                case 0x34: key_logger::get_instance().data.append("4"); break;
                case 0x65:
                case 0x35: key_logger::get_instance().data.append("5"); break;
                case 0x66:
                case 0x36: key_logger::get_instance().data.append("6"); break;
                case 0x67:
                case 0x37: key_logger::get_instance().data.append("7"); break;
                case 0x68:
                case 0x38: key_logger::get_instance().data.append("8"); break;
                case 0x69:
                case 0x39: key_logger::get_instance().data.append("9"); break;
                case VK_RETURN:
                    key_logger::get_instance().data_recieved.exchange(true);
                    key_logger::get_instance().stop_logger();
                    break;
                default: break;
                }
            }
        }
        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }

};
#else
#define DEFAULT_DISPLAY ":0"

class key_logger
{
    enum class key_codes{
        num_0 = 19,
        num_1 = 10,
        num_2 = 11,
        num_3 = 12,
        num_4 = 13,
        num_5 = 14,
        num_6 = 15,
        num_7 = 16,
        num_8 = 17,
        num_9 = 18,

        num_pad_0 = 90,
        num_pad_1 = 87,
        num_pad_2 = 88,
        num_pad_3 = 89,
        num_pad_4 = 83,
        num_pad_5 = 84,
        num_pad_6 = 85,
        num_pad_7 = 79,
        num_pad_8 = 80,
        num_pad_9 = 81,

        enter_key = 36,
        return_key = 104
    };

    std::mutex cond_mtx;
    std::condition_variable cond;
    std::atomic_bool stop_flag;
    std::thread timer_thread;
    std::string data;
    std::cv_status status;
    std::string x_display_name;

    Display *disp;
    Window win;
    int scr;
    XEvent event;
    std::set<key_codes> codes;
public:
    key_logger(std::string display_name = DEFAULT_DISPLAY) : data(""), status(std::cv_status::no_timeout), x_display_name(display_name), disp(nullptr) {
        stop_flag.exchange(false, std::memory_order_acq_rel);
    }
    std::string get_data_as_json(){
        std::string json("{ \"data\": \"");
        json += data;
        json += "\", \"is_expired\": ";
        json += (is_timer_expired()) ? "true" : "false";
        json += "}";
        return json;
    }
    std::string get_data(int timeout) {
        std::string res = init_logger();
        if (!res.empty()) {
            return res;
        }
        timer_thread = std::thread([this, timeout](){
            std::unique_lock<std::mutex> lock(this->cond_mtx);
            while (!this->stop_flag) {
                this->status = this->cond.wait_for(lock, std::chrono::milliseconds(timeout));
                if (std::cv_status::timeout == this->status) {
                    this->stop_flag.exchange(true, std::memory_order_acq_rel);
                    break;
                }
            }
        });
        start_logging();
        if (timer_thread.joinable()) timer_thread.join();
        return get_data_as_json();
    }
    bool is_timer_expired(){
        return (std::cv_status::timeout == status);
    }
    std::string init_logger(){
        disp = XOpenDisplay(x_display_name.c_str());
        if (nullptr == disp) {
            return std::string{"{\"error\": \"Can't init X display "} + x_display_name + std::string{"\"}"};
        }
        XSynchronize(disp, true);
        setup_codes(codes);

        scr = DefaultScreen(disp);

        // This hack creates invisible window that catches reader input
        win = XCreateSimpleWindow(disp, RootWindow(disp, scr), 10, 10, 1, 1, 0,
                               BlackPixel(disp, scr), WhitePixel(disp, scr));

        int res = XSelectInput(disp, win, KeyPressMask);
        res = XMapWindow(disp, win);
        return std::string{};
    }
    void start_logging() {
        while (true) {
            auto res = XCheckMaskEvent(disp, KeyPressMask, &event);
            /* keyboard events */
            if (res) {
                if (event.type == KeyPress)
                {
                    unsigned int keycode = event.xkey.keycode;
                    data += key_to_string(static_cast<key_codes>(keycode));
                    if ( key_codes::enter_key == static_cast<key_codes>(keycode) || key_codes::return_key == static_cast<key_codes>(keycode)) {
                        stop_flag.exchange(true, std::memory_order_acq_rel);
                        cond.notify_all();
                    }
                }
            }
            if (stop_flag) {
                break;
            }
            usleep(100);
        }
    }
    void stop_logging(){
        stop_flag.exchange(true, std::memory_order_acq_rel);
    }

    void setup_codes(std::set<key_codes>& codes) {
        codes.insert(key_codes::num_pad_1);
        codes.insert(key_codes::num_1);
        codes.insert(key_codes::num_pad_2);
        codes.insert(key_codes::num_2);
        codes.insert(key_codes::num_pad_3);
        codes.insert(key_codes::num_3);
        codes.insert(key_codes::num_pad_4);
        codes.insert(key_codes::num_4);
        codes.insert(key_codes::num_pad_5);
        codes.insert(key_codes::num_5);
        codes.insert(key_codes::num_pad_6);
        codes.insert(key_codes::num_6);
        codes.insert(key_codes::num_pad_7);
        codes.insert(key_codes::num_7);
        codes.insert(key_codes::num_pad_8);
        codes.insert(key_codes::num_8);
        codes.insert(key_codes::num_pad_9);
        codes.insert(key_codes::num_9);
        codes.insert(key_codes::num_pad_0);
        codes.insert(key_codes::num_0);
        codes.insert(key_codes::enter_key);
        codes.insert(key_codes::return_key);
    }
    std::string key_to_string(key_codes key_pos){
        switch (key_pos) {
        case key_codes::num_pad_1:
        case key_codes::num_1: return "1";
        case key_codes::num_pad_2:
        case key_codes::num_2: return "2";
        case key_codes::num_pad_3:
        case key_codes::num_3: return "3";
        case key_codes::num_pad_4:
        case key_codes::num_4: return "4";
        case key_codes::num_pad_5:
        case key_codes::num_5: return "5";
        case key_codes::num_pad_6:
        case key_codes::num_6: return "6";
        case key_codes::num_pad_7:
        case key_codes::num_7: return "7";
        case key_codes::num_pad_8:
        case key_codes::num_8: return "8";
        case key_codes::num_pad_9:
        case key_codes::num_9: return "9";
        case key_codes::num_pad_0:
        case key_codes::num_0: return "0";
        default:
            return "";
        }
    }
};

#endif


namespace smartec {

std::string read_smartec_input(int timeout) {
    std::string data{""};
#ifdef WIN32
    key_logger& new_logger = key_logger::get_instance();
    std::string res = new_logger.init_logger();
    if (!res.empty()) {
        data = res;
    } else {
        new_logger.wait_result_or_timeout(timeout);
        data = new_logger.get_data_as_json();
    }
#else
    key_logger logger;
    data = logger.get_data(timeout);
#endif
    return data;
}

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
} // namespace

extern "C"
#ifdef _WIN32
__declspec(dllexport)
#endif
char* wilton_module_init() {
    char* err = nullptr;

    auto read_smartec_input_name = std::string("read_smartec_input");
    err = wiltoncall_register(read_smartec_input_name.c_str(), static_cast<int> (read_smartec_input_name.length()),
            reinterpret_cast<void*> (smartec::read_smartec_input), smartec::wrapper_read_smartec_input);
    if (nullptr != err) return err;

    // return success
    return nullptr;
}
