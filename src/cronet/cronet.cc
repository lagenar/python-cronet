#include <pybind11/pybind11.h>
#include <iostream>
#include "cronet_c.h"
#include "executor.h"
#include "url_request_callback.h"


namespace py = pybind11;

  
Cronet_EnginePtr CreateCronetEngine() {
  Cronet_EnginePtr cronet_engine = Cronet_Engine_Create();
  Cronet_EngineParamsPtr engine_params = Cronet_EngineParams_Create();
  Cronet_EngineParams_user_agent_set(engine_params, "CronetSample/1");
  Cronet_EngineParams_enable_quic_set(engine_params, true);

  Cronet_Engine_StartWithParams(cronet_engine, engine_params);
  Cronet_EngineParams_Destroy(engine_params);
  return cronet_engine;
}


const std::string PerformRequest(Cronet_EnginePtr cronet_engine, const std::string& url, Cronet_ExecutorPtr executor) {
    UrlRequestCallback url_request_callback;
    Cronet_UrlRequestPtr request = Cronet_UrlRequest_Create();
    Cronet_UrlRequestParamsPtr request_params = Cronet_UrlRequestParams_Create();
    Cronet_UrlRequestParams_http_method_set(request_params, "GET");

    Cronet_UrlRequest_InitWithParams(
        request, cronet_engine, url.c_str(), request_params,
        url_request_callback.GetUrlRequestCallback(), executor);
    Cronet_UrlRequestParams_Destroy(request_params);

    Cronet_UrlRequest_Start(request);

    url_request_callback.WaitForDone();
    Cronet_UrlRequest_Destroy(request);

    return url_request_callback.response_as_string();
}


const std::string get(std::string& url) 
{
    Cronet_EnginePtr cronet_engine = CreateCronetEngine();
    std::cout << "Cronet version: " << Cronet_Engine_GetVersionString(cronet_engine) << std::endl;

    std::cout << "URL: " << url << std::endl;
    Executor executor;
    const std::string response = PerformRequest(cronet_engine, url, executor.GetExecutor());

    Cronet_Engine_Shutdown(cronet_engine);
    Cronet_Engine_Destroy(cronet_engine);

    return response;
}



PYBIND11_MODULE(cronet, m) {
    m.doc() = "cronet";

    m.def("get", &get, "Get url");
}
