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
#endif

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

  //   void timer(int timeout) {
		// timer_thread = std::thread([this, timeout](){
  //           std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
  //           std::cout << "timer expired " << GetProcessId(NULL) << std::endl;
		// 	//this->stop_logger();
		// 	get_instance().stop_logger();
  //       });
  //   }

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

int main(int argc, char const *argv[])
{
    std::string data{""};

    std::thread input([&data](){
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
        //Below cin operation should be executed within stipulated period of time
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(STDIN_FILENO, &readSet);
        struct timeval tv = {5, 0};  // 5 seconds, 0 microseconds;
        if (select(STDIN_FILENO+1, &readSet, NULL, NULL, &tv) < 0) std::perror("select");

        if (FD_ISSET(STDIN_FILENO, &readSet)) {
            std::cin >> data;
        }
#endif
    });

    
    if (input.joinable()) {
       input.join();
    }
    std::cout  << "you entered: " << data << std::endl;

	return 0;
}
