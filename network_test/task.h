#ifndef TASK_H_
#define TASK_H_

namespace mydb
{
class Task{
public:
    Task() {}

    virtual ~Task() {}

    virtual void Run() = 0;
};

} // namespace mydb


#endif  