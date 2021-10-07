#include "include/headers.h"

#include <iostream>

int main() {
    mydb::Status s;
    mydb::Database database("local_test_db");
    s = database.Open();

    // Test 1:
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

    if(count == 2) std::cout << "Passed simple local test 1!" << std::endl;
    else std::cout << "An error occurred!" << std::endl;

    // Test 2:
    database.Close();
    database.Open();

    count = 0;
    s = database.Get(key1, &ret_val);
    if(s.IsOK()) count+= 1;
    std::cout << key1 << " : " << ret_val << std::endl;
    s = database.Get(key2, &ret_val);
    if(s.IsOK()) count+= 1;
    std::cout << key2 << " : " << ret_val << std::endl;

    if(count == 2) std::cout << "Passed simple local test 2!" << std::endl;
    else std::cout << "An error occurred!" << std::endl;

    // Test 3:
    s = database.Put(key1, "v1__");
    s = database.Put(key2, "v2__");

    database.Compact();

    count = 0;
    s = database.Get(key1, &ret_val);
    if(s.IsOK()) count+= 1;
    std::cout << key1 << " : " << ret_val << std::endl;
    s = database.Get(key2, &ret_val);
    if(s.IsOK()) count+= 1;
    std::cout << key2 << " : " << ret_val << std::endl;

    if(count == 2) std::cout << "Passed simple local test 3!" << std::endl;
    else std::cout << "An error occurred!" << std::endl;

    // Test 4:
    database.Close();
    database.Open();
    s = database.Put(key1, value1);
    s = database.Put(key2, value2);

    count = 0;
    s = database.Get(key1, &ret_val);
    if(s.IsOK()) count+= 1;
    std::cout << key1 << " : " << ret_val << std::endl;
    s = database.Get(key2, &ret_val);
    if(s.IsOK()) count+= 1;
    std::cout << key2 << " : " << ret_val << std::endl;

    if(count == 2) std::cout << "Passed simple local test 4!" << std::endl;
    else std::cout << "An error occurred!" << std::endl;

    // Test 2:
    database.Close();
    database.Open();

    count = 0;
    s = database.Get(key1, &ret_val);
    if(s.IsOK()) count+= 1;
    std::cout << key1 << " : " << ret_val << std::endl;
    s = database.Get(key2, &ret_val);
    if(s.IsOK()) count+= 1;
    std::cout << key2 << " : " << ret_val << std::endl;

    if(count == 2) std::cout << "Passed simple local test 5!" << std::endl;
    else std::cout << "An error occurred!" << std::endl;

    // Test 3:
    s = database.Put(key1, "v1__");
    s = database.Put(key2, "v2__");

    database.Compact();

    count = 0;
    s = database.Get(key1, &ret_val);
    if(s.IsOK()) count+= 1;
    std::cout << key1 << " : " << ret_val << std::endl;
    s = database.Get(key2, &ret_val);
    if(s.IsOK()) count+= 1;
    std::cout << key2 << " : " << ret_val << std::endl;

    if(count == 2) std::cout << "Passed simple local test 6!" << std::endl;
    else std::cout << "An error occurred!" << std::endl;


    return 0; 
}