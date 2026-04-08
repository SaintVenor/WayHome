#include <iostream>
#include <exception>

#ifdef _WIN32
    #include <windows.h>
    #include <cstdio>
#endif

#include "cache.h"
#include "input_manager.h"

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    setvbuf(stdout, nullptr, _IOFBF, 1000);
#endif

    try {
        CacheCities city_cache;
        InputManager manager(argc, argv, city_cache);
        manager.Run();

    } catch (const std::exception& e) {
        std::cerr << "Ошибка: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
