#include <event.h>
#include <evhttp.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#include <can_protocol.h>
#include <adbk.h>
#include <http.h>
#include <device_container.h>
#include <timed_event.h>
#include <can_handler.h>
#include <platform.h>
#include <config.h>
#include <log.h>

#define HTTP_PORT	4000
#define RESTART_COMMAND "sh $RESTART"
	
/* ---------------------------------------- */	

static DC container;
static event can_event;

static const uint32_t CAN_RESTART_LIMIT = 12;
static uint32_t restart_counter = 0;
static int check_restart(DC& container)
{
        xlog2("checking restart conditions");
        if(container.activeCount() == 0) {
            xlog2("restart conditions satisfied[%i]",restart_counter);
            if(++restart_counter > CAN_RESTART_LIMIT) {
                xlog2("restart server");
                uninit_can_handler(&can_event);
                save_devices(container);
                sleep(10);
                system("reboot");
            }

            xlog2("can reset");
            uninit_can_handler(&can_event);
            init_can_handler(&can_event,&container);
        }
	return 0;
}

/* ---------------------------------------- */

#define TEV_SIZE(e) (sizeof(e)/sizeof(timed_event))

static timed_event can_events[] = {
    { { 1,0},{ 0 ,0}, get_sn },
    { { 2,0},{ 3 ,3}, get_time },
//  { { 2,0},{ 3 ,2}, get_stoplist_version },
    { { 2,0},{ 3 ,1}, get_version },
    { { 2,0},{23,12}, set_time },
    { { 3,0},{ 0 ,0}, get_last_event_id },
    { { 4,0},{ 0 ,0}, get_events }
};

static int plan(DC&)
{
    xlog("plan");
    event_add_timed_events(can_events,TEV_SIZE(can_events),COUNTER_EVENT);
    return 0;
}

extern int execute_file(const char* path,char* const * args);
static int backup(DC&)
{
    xlog2("backup");
    const char* path = "/mnt/cf/nsv/backup";
    char* const args[] = { 0 };
    return execute_file(path,args);
}

static timed_event service_events[] = {
    { {   60, 0},{  600, 0}, save_devices },
    { {    0, 0},{    5, 0}, plan }, //regular service function that plans execution of can events
    { { 3600, 0},{ 3600, 0}, check_restart },
    { {    0, 0},{    3, 0}, adbk_service  },
    { {  600, 0},{  600, 0}, backup }
};


/* ---------------------------------------- */

static void sigchld_handler(int sig)
{
    /* Wait for all dead processes.
     * We use a non-blocking call to be sure this signal handler will not
     * block if a child was cleaned up in another part of the program. */
    while (waitpid(-1, NULL, WNOHANG) > 0) {}
}

static void uninit_handler(int sig)
{
    xlog2("uninit_handler for sig[%i]",sig);
    platform::uninit();
    uninit_can_handler(&can_event);
    save_devices(container);
    exit(5);
}

static int install_signal_handlers()
{
    //ignore SIGPIPE signal
    if( signal(SIGPIPE,SIG_IGN) == SIG_ERR ) {
        xlog2("signal(SIGPIPE,SIG_IGN) == SIG_ERR");
        return -1;
    }

    /*signal(SIGCHLD,sigchld_handler);
    signal(SIGINT,uninit_handler);
    signal(SIGTERM,uninit_handler);*/

    //prevent zombie processes emerging using wait/3
    struct sigaction sigchld_action;
    memset (&sigchld_action, 0, sizeof(sigchld_action));
    sigchld_action.sa_handler = sigchld_handler;
    if (sigaction(SIGCHLD, &sigchld_action, 0)) {
        xlog2("sigaction SIGCHLD: %s",strerror(errno));
        return -2;
    }

    //guarantee correct uninit
    struct sigaction sigint_action;
    memset(&sigint_action,0,sizeof(sigint_action));
    sigint_action.sa_handler = uninit_handler;
    if(sigaction(SIGINT,&sigint_action,0)) {
        xlog2("sigaction SIGINT: %s",strerror(errno));
    }

    struct sigaction sigterm_action;
    memset(&sigterm_action,0,sizeof(sigterm_action));
    sigterm_action.sa_handler = uninit_handler;
    if(sigaction(SIGTERM,&sigterm_action,0)) {
        xlog2("sigaction SIGSTP: %s",strerror(errno));
    }

    return 0;
}

int main(int argc, char* argv[])
{
        install_signal_handlers();

        if( platform::init() == -1 ) return 1;
        if(container.state()) return 2;

        event_base* ev_base = event_init();
	
        init_can_handler(&can_event,&container);

        http_config config = { &container };
        setup_http_server(ev_base,&config);
	
	//timed events setup
        event_set_timed_events(service_events,TEV_SIZE(service_events),&container);
        event_set_timed_events(can_events,TEV_SIZE(can_events),&container);

        event_add_timed_events(service_events,TEV_SIZE(service_events),NORMAL_EVENT);
	
	event_dispatch();

        platform::uninit();
	
	return 0;
}
