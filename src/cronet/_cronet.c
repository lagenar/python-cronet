#define PY_SSIZE_T_CLEAN
#include <pthread.h>
#include <stdbool.h>

#include "Python.h"
#include "cronet_c.h"


#define DEBUG 1

#if DEBUG
#define LOG(message) printf("DEBUG: %s\n", message)
#else
#define LOG(message)
#endif


/* saves the cronet runnable to execute next for a single executor 
   cronet runables are the atomic steps of a request.
*/
typedef struct {
  Cronet_RunnablePtr runnable;
  pthread_mutex_t runnable_mutex;
  pthread_cond_t cond;
  pthread_mutex_t cond_mutex;
  volatile bool should_stop;
} ExecutorRunnable;


/* callback passed to cronet to schedule a runnable. 
   gets the executor context and updates the runnable.
*/
void execute_runnable(Cronet_ExecutorPtr executor,
                      Cronet_RunnablePtr runnable) {
  ExecutorRunnable *run =
      (ExecutorRunnable *)Cronet_Executor_GetClientContext(executor);

  bool updated = false;
  while (!updated) {
    pthread_mutex_lock(&run->runnable_mutex);
    if (run->runnable == NULL) {
      run->runnable = runnable;
      updated = true;
    }
    pthread_cond_signal(&run->cond);
    pthread_mutex_unlock(&run->runnable_mutex);
  }
}

/* executor thread that waits for runnables and starts them.
*/
void *process_requests(void *runnable) {
  ExecutorRunnable *run = (ExecutorRunnable *)runnable;
  while (!run->should_stop) {
    pthread_cond_wait(&run->cond, &run->cond_mutex);
    pthread_mutex_lock(&run->runnable_mutex);
    if (run->runnable != NULL) {
      Cronet_Runnable_Run(run->runnable);
      Cronet_Runnable_Destroy(run->runnable);
      run->runnable = NULL;
    }
    pthread_mutex_unlock(&run->runnable_mutex);
  }
  printf("Returning from thread\n");
  return NULL;
}

void on_redirect_received(Cronet_UrlRequestCallbackPtr callback,
                          Cronet_UrlRequestPtr request,
                          Cronet_UrlResponseInfoPtr info,
                          Cronet_String newLocationUrl) {
  PyObject *py_request = (PyObject*)Cronet_UrlRequest_GetClientContext(request); 
  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();
  PyObject_CallMethod(py_request, "on_redirect_received", 
                      "s", newLocationUrl);
  PyGILState_Release(gstate);
  Cronet_UrlRequest_FollowRedirect(request);
}

void on_response_started(Cronet_UrlRequestCallbackPtr callback,
                         Cronet_UrlRequestPtr request,
                         Cronet_UrlResponseInfoPtr info) {
  PyObject *py_request = (PyObject*)Cronet_UrlRequest_GetClientContext(request); 
  int status_code = Cronet_UrlResponseInfo_http_status_code_get(info);
  int headers_size = Cronet_UrlResponseInfo_all_headers_list_size(info);
  const char *url = Cronet_UrlResponseInfo_url_get(info);
  
  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();
  PyObject *headers = PyDict_New();
  for (int i=0; i < headers_size; i++) {
      Cronet_HttpHeaderPtr header = Cronet_UrlResponseInfo_all_headers_list_at(info, i);
      const char *key = Cronet_HttpHeader_name_get(header);
      const char *value = Cronet_HttpHeader_value_get(header);
      PyObject *item = PyUnicode_FromStringAndSize(value, strlen(value));
      PyDict_SetItemString(headers, key, item);
  }
  PyObject_CallMethod(py_request, "on_response_started", "siO", 
                      url, status_code, headers);
  PyGILState_Release(gstate);
  
  Cronet_BufferPtr buffer = Cronet_Buffer_Create();
  Cronet_Buffer_InitWithAlloc(buffer, 32 * 1024);
  Cronet_UrlRequest_Read(request, buffer);
}

void on_read_completed(Cronet_UrlRequestCallbackPtr callback,
                       Cronet_UrlRequestPtr request,
                       Cronet_UrlResponseInfoPtr info, Cronet_BufferPtr buffer,
                       uint64_t bytesRead) {
  PyObject *py_request = (PyObject*)Cronet_UrlRequest_GetClientContext(request);
  const char *buf_data = (const char*)Cronet_Buffer_GetData(buffer); 
  
  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();
  PyObject *data = PyBytes_FromStringAndSize(buf_data, bytesRead);
  PyObject_CallMethod(py_request, "on_read_completed", "O", data);
  PyGILState_Release(gstate);
  Cronet_UrlRequest_Read(request, buffer);
}

void on_succeeded(Cronet_UrlRequestCallbackPtr callback,
                  Cronet_UrlRequestPtr request,
                  Cronet_UrlResponseInfoPtr info) {
  PyObject *py_request = (PyObject*)Cronet_UrlRequest_GetClientContext(request);
  
  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();
  PyObject_CallMethod(py_request, "on_succeeded", NULL);
  Py_DECREF(py_request);
  PyGILState_Release(gstate);
  
  Cronet_UrlRequest_Destroy(request);
}

void on_failed(Cronet_UrlRequestCallbackPtr callback,
               Cronet_UrlRequestPtr request, Cronet_UrlResponseInfoPtr info,
               Cronet_ErrorPtr error) {
  
  PyObject *py_request = (PyObject*)Cronet_UrlRequest_GetClientContext(request);
  const char *error_msg = Cronet_Error_message_get(error);
  
  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();
  PyObject_CallMethod(py_request, "on_failed", "s", error_msg);
  Py_DECREF(py_request);
  PyGILState_Release(gstate);
  
  Cronet_UrlRequest_Destroy(request);
}

void on_canceled(Cronet_UrlRequestCallbackPtr callback,
                 Cronet_UrlRequestPtr request, Cronet_UrlResponseInfoPtr info) {
  PyObject *py_request = (PyObject*)Cronet_UrlRequest_GetClientContext(request);
  
  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();
  PyObject_CallMethod(py_request, "on_canceled", NULL);
  Py_DECREF(py_request);
  PyGILState_Release(gstate);
  
  Cronet_UrlRequest_Destroy(request);
}


int64_t request_content_length(Cronet_UploadDataProviderPtr self)
{
    Cronet_UploadDataProvider_GetClientContext(self);
    void *content = Cronet_UploadDataProvider_GetClientContext(self);
    return strlen((const char*)content);
}

void request_content_read(Cronet_UploadDataProviderPtr self, 
                          Cronet_UploadDataSinkPtr upload_data_sink, 
                          Cronet_BufferPtr buffer)
{
    size_t buffer_size = Cronet_Buffer_GetSize(buffer);
    void *content = Cronet_UploadDataProvider_GetClientContext(self);
    size_t content_size = strlen((const char*)content);
    assert(buffer_size >= content_size);

    memcpy(Cronet_Buffer_GetData(buffer), content, content_size);
    Cronet_UploadDataSink_OnReadSucceeded(upload_data_sink, content_size, false);
}

void request_content_rewind(Cronet_UploadDataProviderPtr self, 
                            Cronet_UploadDataSinkPtr upload_data_sink)
{
}

void request_content_close(Cronet_UploadDataProviderPtr self)
{
}

typedef struct {
  PyObject_HEAD 
  Cronet_EnginePtr engine;
  Cronet_ExecutorPtr executor;
  ExecutorRunnable executor_runnable;
  pthread_t executor_thread;
} CronetEngineObject;


static void CronetEngine_dealloc(CronetEngineObject *self) {
  self->executor_runnable.should_stop = true;
  pthread_join(self->executor_thread, NULL);
  Cronet_Executor_Destroy(self->executor);
  Cronet_Engine_Shutdown(self->engine);
  Cronet_Engine_Destroy(self->engine);
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static int CronetEngine_init(CronetEngineObject *self, PyObject *args, PyObject *kwds) {
  self->engine = Cronet_Engine_Create();
  Cronet_EngineParamsPtr engine_params = Cronet_EngineParams_Create();
  Cronet_EngineParams_http_cache_mode_set(
      engine_params, Cronet_EngineParams_HTTP_CACHE_MODE_DISABLED);
  Cronet_EngineParams_enable_quic_set(engine_params, false);
  Cronet_EngineParams_user_agent_set(engine_params, "Cronet");
  Cronet_Engine_StartWithParams(self->engine, engine_params);
  Cronet_EngineParams_Destroy(engine_params);
  self->executor = Cronet_Executor_CreateWith(&execute_runnable);
  self->executor_runnable = (ExecutorRunnable){
      .runnable = NULL,
      .cond = PTHREAD_COND_INITIALIZER,
      .cond_mutex = PTHREAD_MUTEX_INITIALIZER,
      .runnable_mutex = PTHREAD_MUTEX_INITIALIZER,
      .should_stop = false,
  };
  Cronet_Executor_SetClientContext(self->executor,
                                   (void *)&self->executor_runnable);
  pthread_t executor_t;
  if (pthread_create(&executor_t, NULL, process_requests,
                     (void *)&self->executor_runnable) != 0) {
    perror("pthread_create failed");
    return 1;
  }

  LOG("Started cronet engine");

  return 0;
}

static PyObject *CronetEngine_request(CronetEngineObject *self, PyObject *args) {
  /* TODO: add validation, destroy request level data after the request is done */
  PyObject *py_request = NULL;
  if (!PyArg_ParseTuple(args, "O", &py_request)) {
    perror("failed to parse arguments\n");
    Py_RETURN_NONE;
  }
  
  Py_INCREF(py_request);
  PyObject* url = PyObject_GetAttrString(py_request, "url");
  PyObject* method = PyObject_GetAttrString(py_request, "method");
  PyObject* content = PyObject_GetAttrString(py_request, "content");
  PyObject* headers = PyObject_GetAttrString(py_request, "headers");
  
  const char *c_url = PyUnicode_AsUTF8(url);
  const char *c_method = PyUnicode_AsUTF8(method);
  char *c_content = NULL;
  if (!Py_IsNone(content)) {
    c_content = PyBytes_AsString(content);
  }

  Cronet_UrlRequestPtr request = Cronet_UrlRequest_Create();
  Cronet_UrlRequestParamsPtr request_params = Cronet_UrlRequestParams_Create();
  Cronet_UrlRequestParams_http_method_set(request_params, c_method);

  if (c_content) {
    Cronet_UploadDataProviderPtr data_provider = 
      Cronet_UploadDataProvider_CreateWith(&request_content_length,
                                           &request_content_read,
                                           &request_content_rewind,
                                           &request_content_close);
    Cronet_UploadDataProvider_SetClientContext(data_provider, c_content);
    Cronet_UrlRequestParams_upload_data_provider_set(request_params, data_provider);
  }
  
  if (!Py_IsNone(headers)) {
    PyObject *items = PyDict_Items(headers);
    Py_ssize_t size = PyList_Size(items);
    for (Py_ssize_t i = 0; i < size; i++) {
      PyObject *item = PyList_GetItem(items, i);
      PyObject *key_obj = PyTuple_GetItem(item, 0);
      PyObject* value_obj = PyTuple_GetItem(item, 1);
      const char* key = PyUnicode_AsUTF8(key_obj);
      const char* value = PyUnicode_AsUTF8(value_obj);
      Cronet_HttpHeaderPtr request_header = Cronet_HttpHeader_Create();
      Cronet_HttpHeader_name_set(request_header, key);
      Cronet_HttpHeader_value_set(request_header, value);
      Cronet_UrlRequestParams_request_headers_add(request_params, request_header);
    }
  }

  Cronet_UrlRequestCallbackPtr callback = Cronet_UrlRequestCallback_CreateWith(
      &on_redirect_received, &on_response_started, &on_read_completed,
      &on_succeeded, &on_failed, &on_canceled);
  
  Cronet_UrlRequest_SetClientContext(request, (void *)py_request);
  Cronet_UrlRequest_InitWithParams(request, self->engine, c_url, request_params,
                                   callback, self->executor);
  Cronet_UrlRequestParams_Destroy(request_params);
  Cronet_UrlRequest_Start(request);

  LOG("Scheduled request");
  return PyCapsule_New(request, NULL, NULL);
}


static PyObject *CronetEngine_cancel(CronetEngineObject *self, PyObject *args) {
  PyObject *capsule = NULL;
  if (!PyArg_ParseTuple(args, "O", &capsule)) {
    perror("failed to parse arguments\n");
    Py_RETURN_NONE;
  }

  void *request = PyCapsule_GetPointer(capsule, NULL);
  if (request) {
    Cronet_UrlRequest_Cancel((Cronet_UrlRequestPtr)request);
  }

  Py_RETURN_NONE;
}


static PyMethodDef CronetEngine_methods[] = {
    {"request", (PyCFunction)CronetEngine_request, METH_VARARGS,
     "Run a request"},
    {"cancel", (PyCFunction)CronetEngine_cancel, METH_VARARGS,
    "Cancel a request"},
    {NULL} /* Sentinel */
};

static PyTypeObject CronetEngineType = {
    .ob_base = PyVarObject_HEAD_INIT(NULL, 0).tp_name = "_cronet.CronetEngine",
    .tp_doc = PyDoc_STR("Cronet engine"),
    .tp_basicsize = sizeof(CronetEngineObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = PyType_GenericNew,
    .tp_init = (initproc)CronetEngine_init,
    .tp_dealloc = (destructor)CronetEngine_dealloc,
    .tp_methods = CronetEngine_methods,
};

static PyModuleDef cronetmodule = {
    .m_base = PyModuleDef_HEAD_INIT,
    .m_name = "cronet",
    .m_doc = "",
    .m_size = -1,
};

PyMODINIT_FUNC PyInit__cronet(void) {
  PyObject *m;
  if (PyType_Ready(&CronetEngineType) < 0)
    return NULL;

  m = PyModule_Create(&cronetmodule);
  if (m == NULL)
    return NULL;

  Py_INCREF(&CronetEngineType);
  if (PyModule_AddObject(m, "CronetEngine", (PyObject *)&CronetEngineType) < 0) {
    Py_DECREF(&CronetEngineType);
    Py_DECREF(m);
    return NULL;
  }

  return m;
}
