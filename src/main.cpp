#include <iostream>
#include <thread>
#include <string>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <atomic>

#ifdef WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/select.h>
#include <poll.h>

#include <X11/Xlib.h>
#include <X11/X.h>

#include <set>
#endif


#ifdef WIN32
class key_logger
{
	std::mutex cond_mtx;
	std::condition_variable cond;

	DWORD thread_id;
	
	HHOOK hook;
	std::thread timer_thread;
public:
	std::atomic_bool data_recieved;
    std::string data;
	key_logger() : data(""), hook(nullptr), thread_id(NULL) {
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
			//std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
			std::unique_lock<std::mutex> lock(this->cond_mtx);
			while (!this->data_recieved) {
				std::cv_status status = this->cond.wait_for(lock, std::chrono::milliseconds(timeout));
				if (std::cv_status::timeout == status) {
					break;
				}
			}
			std::cout << "timer expired " << GetCurrentProcessId() << std::endl;
			get_instance().stop_logger();
		});
        GetMessage(&message, NULL, 0, 0);
        TranslateMessage(&message);
        DispatchMessage(&message);
		if (timer_thread.joinable()) timer_thread.join();
        return data_recieved;
    }

    std::string init_logger(){
        data.clear();
        hook = SetWindowsHookEx(
            WH_KEYBOARD_LL,
			static_cast<HOOKPROC>(&key_logger::keyboard_hook_proc),
            GetModuleHandle(NULL), // current HInstance
            0);
        if (hook == nullptr) {
            return std::string("Init keyboard hook error: " + std::to_string(GetLastError()));
        }
		return std::string{};
    }

    void stop_logger() {
		std::cout << "stop_logger called from " << GetCurrentProcessId() << std::endl;
        if (nullptr != hook) {
            UnhookWindowsHookEx(hook);
			//PostMessage(HWND_BROADCAST, NULL, 0, 0);
			//PostMessage(NULL, NULL, 0, 0);
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
                case VK_RETURN: key_logger::get_instance().data_recieved.exchange(true); key_logger::get_instance().stop_logger(); 
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
#define BIT(c, x)   ( c[x/8]&(1<<(x%8)) )
/* I think it is pretty standard */
#define SHIFT_INDEX 1  /*index for XKeycodeToKeySym(), for shifted keys*/
#define MODE_INDEX 2
#define MODESHIFT_INDEX 3
#define ISO3_INDEX 4 //TODO geht leider nicht??
#define ISO3SHIFT_INDEX 4
#define DEFAULT_DELAY   10000

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
    std::set<key_codes> codes;
public:
    key_logger(std::string display_name = ":0") : data(""), status(std::cv_status::no_timeout), x_display_name(display_name), disp(nullptr) {
        stop_flag.exchange(false, std::memory_order_acq_rel);
    }
    ~key_logger() {
        stop_logging();
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
        return data;
    }
    bool is_timer_expired(){
        return (std::cv_status::timeout == status);
    }
    std::string init_logger(){
        disp = XOpenDisplay(x_display_name.c_str()); // default display
        if (nullptr == disp) {
            return std::string{"{\"error\": \"Can't init X display "} + x_display_name + std::string{"\"}"};
        }
        XSynchronize(disp, true);
        setup_codes(codes);
        return std::string{};
    }
    void start_logging(){
        char* saved, *keys, *char_ptr;
        char buf1[32], buf2[32];
        saved = static_cast<char*> (buf1);
        keys = static_cast<char*> (buf2);
        XQueryKeymap(disp, saved);
        while (true) {
            /* find changed keys */
            XQueryKeymap(disp, keys);
            for (auto& key_code : codes) {
                int key_code_pos = static_cast<int> (key_code);
                if (BIT(keys, key_code_pos)!=BIT(saved, key_code_pos)) {
                    // Если отличается бит то смотрим клавиша нажата или отпущена
                    int code = BIT(keys, key_code_pos);
                    if (code) { // нажата
                        data += key_to_string(key_code);
                        if (key_codes::enter_key == key_code || key_codes::return_key == key_code) {
                            stop_logging();
                        }
                    }
                }
             }

            /* swap buffers */
            char_ptr=saved;
            saved=keys;
            keys=char_ptr;
            usleep(2000);
            if (stop_flag) {
                break;
            }
        }
    }
    void stop_logging(){
        stop_flag.exchange(true, std::memory_order_acq_rel);
        cond.notify_all();
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


int main(int argc, char const *argv[])
{
    std::string data{""};
    int delay=DEFAULT_DELAY;

    std::atomic_bool exit_flag(false);

    std::thread input([&data, &exit_flag, delay](){
        std::cout  << "enter symbols:" << std::endl;

#ifdef WIN32

        key_logger& new_logger = key_logger::get_instance();
		std::string res = new_logger.init_logger();
		if (res.empty()) {
    		if (new_logger.wait_result_or_timeout(2000)) {
                std::cout << "Result: " << new_logger.data << std::endl;
                data = new_logger.data;
            } else {
                std::cout << "Timeout expired" << std::endl;
            }
        } else {
            std::cout << res << std::endl;            
        }

		std::cout << "Result: " << new_logger.data << std::endl;
#else
        key_logger logger;
        data = logger.get_data(2000);

#endif
    });

    if (input.joinable()) {
       input.join();
    }
    std::cout  << "you entered: " << data << std::endl;

	return 0;
}
