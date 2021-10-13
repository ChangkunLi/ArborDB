#ifndef TASK_H_
#define TASK_H_

#include <thread>

namespace mydb
{
class Task{
public:
    Task() {}

    virtual ~Task() {}

    virtual void Run(std::thread::id tid) = 0;
};

} // namespace mydb


#endif  