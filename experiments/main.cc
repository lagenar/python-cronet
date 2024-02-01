// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <vector>

#include "cronet_c.h"
#include "sample_executor.h"
#include "sample_url_request_callback.h"

Cronet_EnginePtr CreateCronetEngine() {
  Cronet_EnginePtr cronet_engine = Cronet_Engine_Create();
  Cronet_EngineParamsPtr engine_params = Cronet_EngineParams_Create();
  Cronet_EngineParams_user_agent_set(engine_params, "CronetSample/1");
  Cronet_EngineParams_enable_quic_set(engine_params, true);

  Cronet_Engine_StartWithParams(cronet_engine, engine_params);
  Cronet_EngineParams_Destroy(engine_params);
  return cronet_engine;
}

void PerformRequest(Cronet_EnginePtr cronet_engine,
                    const std::string& url,
                    Cronet_ExecutorPtr executor) {
  const int requests_count = 100;
  std::vector<SampleUrlRequestCallback*> callbacks;
  std::vector<Cronet_UrlRequestPtr> requests;

  for (int i = 0; i < requests_count; i++) {
    SampleUrlRequestCallback* url_request_callback = new SampleUrlRequestCallback();
    Cronet_UrlRequestPtr request = Cronet_UrlRequest_Create();
    Cronet_UrlRequestParamsPtr request_params = Cronet_UrlRequestParams_Create();
    Cronet_UrlRequestParams_http_method_set(request_params, "GET");

    Cronet_UrlRequest_InitWithParams(
        request, cronet_engine, url.c_str(), request_params,
        url_request_callback->GetUrlRequestCallback(), executor);
    Cronet_UrlRequestParams_Destroy(request_params);

    Cronet_UrlRequest_Start(request);
    callbacks.push_back(url_request_callback);
    requests.push_back(request);
  }

  for (int i = 0; i < requests_count; i++) {
    (*callbacks[i]).WaitForDone();
    Cronet_UrlRequest_Destroy(requests[i]);

    //std::cout << "Response Data:" << std::endl
    //        << (*callbacks[i]).response_as_string() << std::endl;
  }

}

// Download a resource from the Internet. Optional argument must specify
// a valid URL.
int main(int argc, const char* argv[]) {
  std::cout << "Hello from Cronet!\n";
  Cronet_EnginePtr cronet_engine = CreateCronetEngine();
  std::cout << "Cronet version: "
            << Cronet_Engine_GetVersionString(cronet_engine) << std::endl;

  std::string url(argc > 1 ? argv[1] : "https://www.example.com");
  std::cout << "URL: " << url << std::endl;
  SampleExecutor executor;
  PerformRequest(cronet_engine, url, executor.GetExecutor());

  Cronet_Engine_Shutdown(cronet_engine);
  Cronet_Engine_Destroy(cronet_engine);
  return 0;
}
