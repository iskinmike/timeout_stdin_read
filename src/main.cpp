#include <iostream>
#include <thread>
#include <string>

#ifdef WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/select.h>
#include <poll.h>
#endif

int main(int argc, char const *argv[])
{
	/* code */

    std::string data{""};

    std::thread input([&data](){
        std::cout  << "enter symbols:" << std::endl;

#ifdef WIN32
        HANDLE event_handle = GetStdHandle(STD_INPUT_HANDLE);

        const DWORD timeout = 5000;
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
