#ifndef __WBE_APPLICATION_GAME_HH__
#define __WBE_APPLICATION_GAME_HH__

#include "function/application/application.hh"
namespace WhiteBirdEngine {

class ApplicationGame : public Application {
public:
    ApplicationGame() = default;
    virtual ~ApplicationGame() {}
    ApplicationGame(const ApplicationGame&) = delete;
    ApplicationGame(ApplicationGame&&) = delete;
    ApplicationGame& operator=(const ApplicationGame&) = delete;
    ApplicationGame& operator=(ApplicationGame&&) = delete;

    virtual void init() override;
    virtual void render_tick(double p_delta) override;
    virtual void logic_tick(double p_delta) override;


};

}

#endif
