#include <pybind11/pybind11.h>
#include <iostream>
#include <cstring>
#include <memory>
#include "cronet_c.h"
#include "executor.h"
#include "url_request_callback.h"

namespace py = pybind11;

int64_t RequestContentLength(Cronet_UploadDataProviderPtr self)
{
    std::string *content = static_cast<std::string *>(Cronet_UploadDataProvider_GetClientContext(self));
    return content->length();
}

void RequestContentRead(Cronet_UploadDataProviderPtr self, Cronet_UploadDataSinkPtr upload_data_sink, Cronet_BufferPtr buffer)
{
    size_t buffer_size = Cronet_Buffer_GetSize(buffer);
    std::cout << "Buffer size " << buffer_size << std::endl;
    std::string *content = static_cast<std::string *>(Cronet_UploadDataProvider_GetClientContext(self));
    memcpy(Cronet_Buffer_GetData(buffer), content->c_str(), content->length());
    Cronet_UploadDataSink_OnReadSucceeded(upload_data_sink, content->length(), false);
}

void RequestContentRewind(Cronet_UploadDataProviderPtr self, Cronet_UploadDataSinkPtr upload_data_sink)
{
}

void RequestContentClose(Cronet_UploadDataProviderPtr self)
{
}

class Cronet
{
public:
    Cronet()
    {
        engine_ = Cronet_Engine_Create();
        Cronet_EngineParamsPtr engine_params = Cronet_EngineParams_Create();
        Cronet_EngineParams_enable_quic_set(engine_params, true);
        /// XXX: start engine outside the constructor?
        Cronet_Engine_StartWithParams(engine_, engine_params);
        Cronet_EngineParams_Destroy(engine_params);
    }

    ~Cronet() 
    {
        Cronet_Engine_Shutdown(engine_);
        Cronet_Engine_Destroy(engine_);
    }

    const Response handle_request(const std::string &url, const std::string &method,
                                  std::string &content, const py::dict &headers, py::object py_callbacks)
    {
        std::unique_ptr<UrlRequestCallback> url_request_callback = std::make_unique<UrlRequestCallback>(py_callbacks);;
        Cronet_UrlRequestPtr request = Cronet_UrlRequest_Create();
        Cronet_UrlRequestParamsPtr request_params = Cronet_UrlRequestParams_Create();
        Cronet_UrlRequestParams_http_method_set(request_params, method.c_str());

        Cronet_UploadDataProviderPtr upload_data_provider = Cronet_UploadDataProvider_CreateWith(
            &RequestContentLength,
            &RequestContentRead,
            &RequestContentRewind,
            &RequestContentClose);
        Cronet_UploadDataProvider_SetClientContext(upload_data_provider, static_cast<void *>(&content));
        Cronet_UrlRequestParams_upload_data_provider_set(request_params, upload_data_provider);

        for (auto header : headers)
        {
            Cronet_HttpHeaderPtr request_header = Cronet_HttpHeader_Create();
            Cronet_HttpHeader_name_set(request_header, std::string(py::str(header.first)).c_str());
            Cronet_HttpHeader_value_set(request_header, std::string(py::str(header.second)).c_str());
            Cronet_UrlRequestParams_request_headers_add(request_params, request_header);
        }

        Cronet_UrlRequest_InitWithParams(
            request, engine_, url.c_str(), request_params,
            url_request_callback->GetUrlRequestCallback(), executor_.GetExecutor());
        Cronet_UrlRequestParams_Destroy(request_params);

        py::gil_scoped_release release;
        Cronet_UrlRequest_Start(request);

        url_request_callback->WaitForDone();
        Cronet_UrlRequest_Destroy(request);
        py::gil_scoped_acquire acquire;
        
        Response response = {};
        response.status_code = url_request_callback->status_code;
        response.content = py::bytes(url_request_callback->response_as_string());
        response.headers = url_request_callback->headers;

        return response;
    }

private:
    Cronet_EnginePtr engine_;
    Executor executor_;
};


PYBIND11_MODULE(cronet, m)
{
    m.doc() = "cronet";
    py::class_<Response>(m, "Response")
        .def_readonly("status_code", &Response::status_code)
        .def_readonly("content", &Response::content)
        .def_readonly("headers", &Response::headers);
    py::class_<Cronet>(m, "Cronet")
        .def(py::init<>())
        .def("handle_request", &Cronet::handle_request, "Perform a request");
}
