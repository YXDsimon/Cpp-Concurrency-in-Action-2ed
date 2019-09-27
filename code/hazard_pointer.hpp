#include <thread>
#include <atomic>

constexpr int maxSize = 100;

struct HazardPointer {
    std::atomic<std::thread::id> id;
    std::atomic<void*> p;
};

HazardPointer a[maxSize];

class HP {
    HazardPointer* hp;
public:
    HP(const HP&) = delete;
    HP operator=(const HP&) = delete;
    HP(): hp(nullptr)
    {
        for(int i = 0; i < maxSize; ++i)
        {
            std::thread::id oldId;
            if(a[i].id.compare_exchange_strong(oldId, std::this_thread::get_id()))
            { // a[i].id == oldId说明a[i]未被设置过，将其设为当前线程ID
                hp = &a[i]; // 将a[i]分配给该线程的hp
                break;
            }
        }
        // 遍历数组都已经被设置过，说明没有新的位置可以分配给当前线程
        if(!hp) throw std::runtime_error("No hazard pointers available");
    }

    std::atomic<void*>& getPointer()
    {
        return hp->p;
    }

    ~HP()
    {
        hp->p.store(nullptr);
        hp->id.store(std::thread::id());
    }
};

std::atomic<void*>& get_HazardPointer_for_current_thread()
{
    thread_local static HP hp; // 每个线程都有各自的hazard pointer
    return hp.getPointer();
}

bool outstanding_hazard_pointers_for(void* x)
{
    for(int i = 0; i < maxSize; ++i)
    {
        if(a[i].p.load() == x) return true;
    }
    return false;
}