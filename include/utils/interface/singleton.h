#ifndef __WBE_SINGLETON_H__
#define __WBE_SINGLETON_H__

#include <stdexcept>
namespace WhiteBirdEngine {

template <typename T>
class Singleton {
public:
    Singleton() {
        if (has_instance) {
            throw std::runtime_error("Failed to construct object: trying to construct multiple singletons.");
        }
        has_instance = true;
    }

    virtual ~Singleton() {
        has_instance = false;
    }

private:
    inline static bool has_instance = false;
};

}

#endif
