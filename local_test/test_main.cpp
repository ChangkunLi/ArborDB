#include "include/headers.h"

#include <iostream>

int main() {
    mydb::Status s;
    mydb::Database database("local_test_db");
    s = database.Open();

    std::string key1("k1");
    std::string key2("k2");
    std::string value1("v1");
    std::string value2("v2");

    s = database.Put(key1, value1);
    s = database.Put(key2, value2);

    std::string ret_val;
    int count = 0;
    s = database.Get(key1, &ret_val);
    if(s.IsOK()) count+= 1;
    std::cout << key1 << " : " << ret_val << std::endl;
    s = database.Get(key2, &ret_val);
    if(s.IsOK()) count+= 1;
    std::cout << key2 << " : " << ret_val << std::endl;

    if(count == 2) std::cout << "Passed simple local test!" << std::endl;
    else std::cout << "An error occurred!" << std::endl;

    return 0; 
}