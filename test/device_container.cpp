#include <can_device.h>
#include <device_container.h>
#include <storage.h>
#include <log.h>

class Dumper : public IDeviceProcessor
{
public:
    static void dump(DC& container) {
        Dumper dumper;
        container.for_each(TYPE_ANY,&dumper);
    }
    void process(device_data *device) {
        xlog2("addr[%hX] sn[%llX] ans[%i]",device->addr,device->sn,device->answered);
    }
};

struct dev_id_t
{
    uint64_t sn;
    uint16_t addr;
};

struct test_case_t
{
    dev_id_t id[100];
} test_cases[] = { { { {0xA,0x1},{0xB,0x2},{0xC,0x3},{0} } } };

int main()
{
    for(size_t i=0;i<sizeof(test_cases)/sizeof(test_case_t);i++) {
        DC container;

        test_case_t *test = &test_cases[i];
        int k = -1;
        while(test->id[++k].sn) {
            dev_id_t id = test->id[k];
            container.answer(id.addr,id.sn);
        }

        Dumper::dump(container);
    }
}
