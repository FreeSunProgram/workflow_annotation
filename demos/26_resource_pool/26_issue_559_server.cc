#include <workflow/WFTask.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFResourcePool.h>
#include <workflow/WFFacilities.h>
#include <workflow/WFHttpServer.h>
#include <signal.h>
#include <unordered_map>
#include <memory>
#include "util.h"

static WFFacilities::WaitGroup wait_group(1);
std::atomic<int> cnt;
std::unordered_map<int, std::unique_ptr<WFResourcePool> > uid_pool_map;

void sig_handler(int signo)
{
    wait_group.done();
}

void go_func(int uid, protocol::HttpResponse *resp)
{
    char buf[256];
    sprintf(buf, "uid is : %d, cnt is %d\n", uid, cnt++);
    resp->append_output_body(buf);
}

int main()
{
    signal(SIGINT, sig_handler);

    WFHttpServer server([](WFHttpTask *server_task)
    {
        auto req = server_task->get_req();
        auto resp = server_task->get_resp();
        const void *body;
        size_t len;
        req->get_parsed_body(&body, &len);

        int uid = atoi(static_cast<const char *>(body));
        fprintf(stderr, "uid : %d\n", uid);
 
        if (!uid_pool_map.count(uid))
        {
            uid_pool_map[uid] = util::make_unique<WFResourcePool>(1);
        }
     
        WFGoTask *go_task = WFTaskFactory::create_go_task("GO", go_func, uid, resp);

        WFConditional *cond = uid_pool_map[uid]->get(go_task, &go_task->user_data);
        series_of(server_task)->push_back(cond);

        server_task->set_callback([&uid](WFHttpTask *)
        { 
            fprintf(stderr, "post resource back\n");
            uid_pool_map[uid]->post(nullptr); 
        });
    });

    if (server.start(8888) == 0)
    {
        wait_group.wait();
        server.stop();
    }

    return 0;
}