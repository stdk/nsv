#include "http.h"

#include <stdio.h>
#include "version.h"
#include "can_device.h"
#include "net.h"

#include "log.h"

#include <stdlib.h>
#include <sys/utsname.h>
#include <arpa/inet.h>

//external definition of http command handler from command.cpp
extern void http_cmd(evhttp_request *request,void *ctx);

#define HTTP_PORT 4000
#define NSV_INDEX_FILENAME_ENV "INDEX"

const char* version_format = "%i.%i.%i [%s %s]";

const char* server_info_format = "%s<div id=\"storage\">Last Event [%i]</div><div id=\"server\">IP [%s][%s]</div>";

void http_main(evhttp_request *request,void *ctx)
{
	xlog("http_main request: %s:%d URI: %s",request->remote_host,request->remote_port,request->uri);	
	
	evbuffer *evb = evbuffer_new();
	
	const char* index_filename = getenv(NSV_INDEX_FILENAME_ENV);
	if(index_filename) {
                long int size = 0;
                char * index_page = get_file_contents(index_filename,&size);
		if(index_page) {
                        evbuffer_add_printf(evb,"%s",index_page);
		} else {
			evbuffer_add_printf(evb,"Error loading index page[%s]",index_filename);
		}
                delete[] index_page;
	} else {
		evbuffer_add_printf(evb,"%s environment variable not specified",NSV_INDEX_FILENAME_ENV);
	}
	
	evhttp_add_header(request->output_headers,"Content-Type","text/html; charset=utf-8");
	evhttp_send_reply(request,HTTP_OK,"Main",evb);
	evbuffer_free(evb);
}

void can_device_plaintext(evbuffer * evb,const device_data * device)
{
        evbuffer_add_printf(evb,"[%p] ",device);
        evbuffer_add_printf(evb,"%i ",device->active());
        evbuffer_add_printf(evb,"%08X ",device->addr);
        evbuffer_add_printf(evb,"%016llX ",device->sn);
        evbuffer_add_printf(evb,"%12s ",device->version);

        char time_buf[30]= {0};
        strftime(time_buf,sizeof(time_buf)-1,"%F %T",localtime(&device->inner_time));
        evbuffer_add_printf(evb,"%s ",time_buf);

        strftime(time_buf,sizeof(time_buf)-1,"%F %T",localtime(&device->last_answer_time));
        evbuffer_add_printf(evb,"%s (%u) ",time_buf,device->answered);
        evbuffer_add_printf(evb,"%8u/%8u ",device->current_event_id,device->last_event_id);
        evbuffer_add_printf(evb,"%8u ",device->errors);
        evbuffer_add_printf(evb,"%8i ",device->progress);
        evbuffer_add_printf(evb,"\n");
}

void http_evb_print_server_info(evbuffer * evb,info * inf)
{
	char server_time[30] = {0};
	time_t now = time(0);
	strftime(server_time,sizeof(server_time)-1,"%F %T",localtime(&now));
	
	struct utsname uts_name;
	strcpy(uts_name.nodename,"<none>");
	uname(&uts_name);
	
        evbuffer_add_printf(evb,server_info_format,server_time,
                            inf->container->eventStorage()->event_id(),
                            uts_name.nodename,
                            inf->ip);//server info
}

void http_info(evhttp_request *request,void *ctx)
{
    xlog("http_info request: %s:%d URI: %s",request->remote_host,request->remote_port,request->uri);
    info * inf = (info*)ctx;

    evbuffer *evb = evbuffer_new();

    http_evb_print_server_info(evb,inf);

    evhttp_add_header(request->output_headers,"Content-Type","text/html; charset=utf-8");
    evhttp_send_reply(request,HTTP_OK,"Devices",evb);
    evbuffer_free(evb);
}

class DevicePrinter : public IDeviceProcessor
{
public:
        typedef void (*print_function)(evbuffer * evb,const device_data * device);

        DevicePrinter(evbuffer* _evb,print_function f,const char* _delim="")
            :evb(_evb),p_function(f),delim(_delim),first(true) {}
        virtual void process(device_data* device) {
            if(!first) evbuffer_add_printf(evb,"%s",delim);
            p_function(evb,device);
            first = false;
        }
private:
        evbuffer *evb;
        print_function p_function;
        const char* delim;
        bool first;
};

void evb_print_version(evbuffer * evb)
{
	evbuffer_add_printf(evb,version_format,nsv_version.major,nsv_version.minor,
                                                nsv_version.fix,nsv_version.build_date,
                                                nsv_version.build_time);
}

void http_version(evhttp_request *request,void *ctx)
{
	xlog("http_version request: %s:%d URI: %s",request->remote_host,request->remote_port,request->uri);	
	
	evbuffer *evb = evbuffer_new();
	evb_print_version(evb);	
	evhttp_add_header(request->output_headers,"Content-Type","text/html; charset=utf-8");
	evhttp_send_reply(request,HTTP_OK,"Version",evb);
	evbuffer_free(evb);
}

void http_debug(evhttp_request *request,void *ctx)
{
        xlog("http_debug request: %s:%d URI: %s",request->remote_host,request->remote_port,request->uri);

        info * inf = (info*)ctx;

        evbuffer *evb = evbuffer_new();

        DevicePrinter printer(evb,can_device_plaintext);

        evbuffer_add_printf(evb,"Active:\n");
        inf->container->for_each_active(TYPE_ANY,&printer);

        evbuffer_add_printf(evb,"All:\n");
        inf->container->for_each(TYPE_ANY,&printer);

        evhttp_add_header(request->output_headers,"Content-Type","text/plain; charset=utf-8");
        evhttp_send_reply(request,HTTP_OK,"Debug",evb);
        evbuffer_free(evb);
}

void can_device_ajax(evbuffer * evb,const device_data * device)
{
    const char *format = "[\"%i\",\"%s\",\"%016llX\",\"%s\",\"%s\",\"%s (%u)\",\"%u/%u\",\"%u\",\"%i\"]";

    char addr[16] = {0};
    switch(device->get_type()) {
    case TYPE_CAN:  snprintf(addr,sizeof(addr),"%X",device->addr); break;
    case TYPE_ADBK: inet_ntop(AF_INET,&device->addr,addr,sizeof(addr)); break;
    }

    char inner_time_buf[30]= {0};
    strftime(inner_time_buf,sizeof(inner_time_buf)-1,"%F %T",localtime(&device->inner_time));
    char last_answer_time_buf[30] = {0};
    strftime(last_answer_time_buf,sizeof(last_answer_time_buf)-1,"%F %T",localtime(&device->last_answer_time));

    evbuffer_add_printf(evb,format,
                        device->active(),
                        addr,
                        device->sn,
                        device->version,
                        inner_time_buf,
                        last_answer_time_buf,
                        device->answered,
                        device->current_event_id,device->last_event_id,
                        device->errors,
                        device->progress);
}

void ajax_devices(evhttp_request *request,void *ctx)
{
    xlog("ajax_devices request: %s:%d URI: %s",request->remote_host,request->remote_port,request->uri);

    info * inf = (info*)ctx;

    evbuffer *evb = evbuffer_new();
    evbuffer_add_printf(evb,"%s","{ \"aaData\":[");

    DevicePrinter printer(evb,can_device_ajax,",");
    inf->container->for_each(TYPE_ANY,&printer);
    evbuffer_add_printf(evb,"%s","]}");

    evhttp_add_header(request->output_headers,"Content-Type","text/html; charset=utf-8");
    evhttp_send_reply(request,HTTP_OK,"ajax_devices",evb);
    evbuffer_free(evb);
}

int setup_http_server(event_base * ev_base,DC * container)
{
        static info inf = { container };

        uint32_t ip = 0;
        if(-1 != get_ip("eth0",&ip)) {
                inet_ntop(AF_INET,&ip,inf.ip,sizeof(inf.ip));
                xlog2("Determined eth0 IP address: %s",inf.ip);

		evhttp *ev_http = evhttp_new(ev_base);
		
                if(0 != evhttp_bind_socket(ev_http,inf.ip,HTTP_PORT)) {
			xlog("evhttp_bind_socket: fail");
		}
		
		evhttp_set_cb(ev_http,"/",http_main,&inf);
                evhttp_set_cb(ev_http,"/info",http_info,&inf);
		evhttp_set_cb(ev_http,"/version",http_version,&inf);
                evhttp_set_cb(ev_http,"/dev-data",ajax_devices,&inf);
		evhttp_set_cb(ev_http,"/cmd",http_cmd,&inf);

                evhttp_set_cb(ev_http,"/debug",http_debug,&inf);
		
		return 0;
	} else {
		xlog2("There is no eth0 interface here: http server is meaningless");
		return -1;
	}
}
