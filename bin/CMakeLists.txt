add_executable(main main.cpp)
target_link_libraries(main PRIVATE cpr::cpr 
    nlohmann_json::nlohmann_json 
    yandex_api_lib
)