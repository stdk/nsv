#include "platform.sys.h"
#include "platform.h"
#include "log.h"

static bool platform_initialized = false;

int platform::init()
{
    if(platform_initialized) {
        xlog2("Platform init: already happened");
        return 0;
    }

    int ret = platform_init();
    if(ret == 0) {
        xlog2("platform init: success");
        platform_initialized = true;
    } else {
        xlog2("Platform init: fail");
    }
    return ret;
}

int platform::uninit()
{
    if(!platform_initialized) {
        xlog2("platform uninit: not needed");
        return 0;
    }

    int ret = platform_uninit();
    if(ret == 0) {
        xlog2("platform uninit: success");
        platform_initialized = false;
    } else {
        xlog2("platform uninit: fail");
    }
    return ret;
}

class BusLock
{
public:
    BusLock() {
        sbuslock();
    }

    ~BusLock() {
        sbusunlock();
    }
};

void red_led(unsigned int state)
{
    BusLock lock;

    if (state) {
        poke16(0x62, peek16(0x62) | 0x4000);
    } else {
        poke16(0x62, peek16(0x62) & ~0x4000);
    }

}

void platform::red_led_switch()
{
    static unsigned int counter = 0;
    BusLock lock;
    if(++counter == 5) {
        poke16(0x62, peek16(0x62) ^ 0x4000);
        counter = 0;
    }
}

void platform::green_led_switch()
{
    BusLock lock;
    poke16(0x62, peek16(0x62) ^ 0x8000);
}

