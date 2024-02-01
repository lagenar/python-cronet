
#include <stdbool.h>
#include "cronet_c.h"
#include <unistd.h>
#include <stdio.h>

void execute(Cronet_ExecutorPtr executor, Cronet_RunnablePtr runnable)
{
    Cronet_Runnable_Run(runnable);
    printf("execute");
    Cronet_Runnable_Destroy(runnable);
}


void on_redirect_received(Cronet_UrlRequestCallbackPtr self, Cronet_UrlRequestPtr request, Cronet_UrlResponseInfoPtr info, Cronet_String new_location_url)
{

}

void on_response_started(Cronet_UrlRequestCallbackPtr self, Cronet_UrlRequestPtr request, Cronet_UrlResponseInfoPtr info)
{
    printf("called started");
}


void on_read_completed(Cronet_UrlRequestCallbackPtr self, Cronet_UrlRequestPtr request, Cronet_UrlResponseInfoPtr info, Cronet_BufferPtr buffer, uint64_t bytes_read) 
{
    
}


void on_succeeded(Cronet_UrlRequestCallbackPtr self, Cronet_UrlRequestPtr request, Cronet_UrlResponseInfoPtr info)
{

}

void on_failed(Cronet_UrlRequestCallbackPtr self, Cronet_UrlRequestPtr request, Cronet_UrlResponseInfoPtr info, Cronet_ErrorPtr error)
{

}

void on_canceled(Cronet_UrlRequestCallbackPtr self, Cronet_UrlRequestPtr request, Cronet_UrlResponseInfoPtr info)
{

}


void perform_request(Cronet_EnginePtr cronet_engine, char* url, Cronet_ExecutorPtr executor) {
  //SampleUrlRequestCallback url_request_callback;
  Cronet_UrlRequestCallbackPtr callback = Cronet_UrlRequestCallback_CreateWith(
    &on_redirect_received, &on_response_started, &on_read_completed, &on_succeeded, &on_failed, &on_canceled);
  Cronet_UrlRequestPtr request = Cronet_UrlRequest_Create();
  Cronet_UrlRequestParamsPtr request_params = Cronet_UrlRequestParams_Create();
  Cronet_UrlRequestParams_http_method_set(request_params, "GET");

  Cronet_UrlRequest_InitWithParams(request, cronet_engine, url, request_params, callback, executor);
  Cronet_UrlRequestParams_Destroy(request_params);

  Cronet_UrlRequest_Start(request);
  //Cronet_UrlRequest_Destroy(request);
}


int main(int argc, const char* argv[]) 
{
    Cronet_EnginePtr cronet_engine = Cronet_Engine_Create();
    Cronet_EngineParamsPtr engine_params = Cronet_EngineParams_Create();
    Cronet_EngineParams_user_agent_set(engine_params, "CronetSample/1");
    Cronet_EngineParams_enable_quic_set(engine_params, true);

    Cronet_Engine_StartWithParams(cronet_engine, engine_params);
    Cronet_ExecutorPtr executor = Cronet_Executor_CreateWith(&execute);

    perform_request(cronet_engine, "https://google.com", executor);

    Cronet_EngineParams_Destroy(engine_params);
    
    //Cronet_Engine_Shutdown(cronet_engine);
    //Cronet_Engine_Destroy(cronet_engine);
    return 0;
}
