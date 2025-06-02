#ifndef __PE_FRAME_H__
#define __PE_FRAME_H__

namespace PencilEngine {

class Frame {
public:
    virtual void init() = 0;
    virtual void update(double p_delta) = 0;
};

}

#endif
