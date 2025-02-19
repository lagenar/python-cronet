#define PY_SSIZE_T_CLEAN
#include <pthread.h>
#include <stdbool.h>
#include <time.h>

#include "Python.h"
#include "cronet_c.h"


#define DEBUG 1

#if DEBUG
#define LOG(message) printf("DEBUG: %s\n", message)
#else
#define LOG(message)
#endif

#undef Py_Is
#undef Py_IsNone

// Export Py_Is(), Py_IsNone() as regular functions
// for the stable ABI.
int Py_Is(PyObject *x, PyObject *y)
{
    return (x == y);
}

int Py_IsNone(PyObject *x)
{
    return Py_Is(x, Py_None);
}


#define RUNNABLES_MAX_SIZE 10000

typedef struct {
    Cronet_RunnablePtr arr[RUNNABLES_MAX_SIZE];
    int front;
    int rear;
    int size;
    pthread_mutex_t mutex;
} Runnables;

void runnables_init(Runnables* q) {
    q->front = 0;
    q->rear = 0;
    q->size = 0;
    pthread_mutex_init(&q->mutex, NULL);
}

void runnables_destroy(Runnables* q) {
    pthread_mutex_destroy(&q->mutex);
}

int runnables_enqueue(Runnables* q, Cronet_RunnablePtr runnable) {
    int status = 0;
    
    pthread_mutex_lock(&q->mutex);
    if (q->size == RUNNABLES_MAX_SIZE) {
        status = -1;
    } else {
        q->arr[q->rear] = runnable;
        q->rear = (q->rear + 1) % RUNNABLES_MAX_SIZE;
        q->size++;
    }
    pthread_mutex_unlock(&q->mutex);
    
    return status;
}

Cronet_RunnablePtr runnables_dequeue(Runnables* q) {
    Cronet_RunnablePtr runnable = NULL;
    
    pthread_mutex_lock(&q->mutex);
    if (q->size > 0) {
        runnable = q->arr[q->front];
        q->front = (q->front + 1) % RUNNABLES_MAX_SIZE;
        q->size--;
    }
    pthread_mutex_unlock(&q->mutex);
    
    return runnable;
}

/* saves the cronet runnable to execute next for a single executor
   cronet runables are the atomic steps of a request.
*/
typedef struct {
  Runnables* runnables;
  pthread_mutex_t mutex;
  pthread_cond_t new_item;
  volatile bool should_stop;
} ExecutorContext;

typedef struct {
  Cronet_UrlRequestCallbackPtr callback;
  PyObject *py_callback;
  bool allow_redirects;
} RequestContext;


typedef struct {
  size_t upload_size;
  size_t upload_bytes_read;
  const char *content;
} UploadProviderContext;

void RequestContext_destroy(RequestContext* ctx)
{
  if (ctx->callback) {
    Cronet_UrlRequestCallback_Destroy(ctx->callback);
  }
  if (ctx->py_callback) {
    Py_DECREF(ctx->py_callback);
  }
  free(ctx);
}

/* callback passed to cronet to schedule a runnable.
   gets the executor context and updates the runnable.
*/
void execute_runnable(Cronet_ExecutorPtr executor,
                      Cronet_RunnablePtr runnable) {
  ExecutorContext *run =
      (ExecutorContext *)Cronet_Executor_GetClientContext(executor);

  pthread_mutex_lock(&run->mutex);
  // TODO: check return value
  runnables_enqueue(run->runnables, runnable);
  pthread_cond_signal(&run->new_item);
  pthread_mutex_unlock(&run->mutex);
}


/* executor thread that waits for runnables and starts them.
*/
void *process_requests(void *executor_context) {
  ExecutorContext *ctx = (ExecutorContext *)executor_context;
  struct timespec ts;
  while (!ctx->should_stop) {
    
    pthread_mutex_lock(&ctx->mutex);
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += 100;
    
    int res = pthread_cond_timedwait(&ctx->new_item, &ctx->mutex, &ts);
    if (res == 0 || res == ETIMEDOUT) {
      Cronet_RunnablePtr runnable = runnables_dequeue(ctx->runnables);
      if (runnable != NULL) {
        Cronet_Runnable_Run(runnable);
        Cronet_Runnable_Destroy(runnable);
      }
    }
    pthread_mutex_unlock(&ctx->mutex);
  }
  return NULL;
}


void on_redirect_received(Cronet_UrlRequestCallbackPtr callback,
                          Cronet_UrlRequestPtr request,
                          Cronet_UrlResponseInfoPtr info,
                          Cronet_String newLocationUrl) {
  RequestContext *ctx = (RequestContext*)Cronet_UrlRequest_GetClientContext(request);
  const char *url = Cronet_UrlResponseInfo_url_get(info);
  int status_code = Cronet_UrlResponseInfo_http_status_code_get(info);
  int headers_size = Cronet_UrlResponseInfo_all_headers_list_size(info);
  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();
  //PyObject *headers = PyDict_New();
  PyObject *headers = PyList_New((Py_ssize_t)0);
  for (int i=0; i < headers_size; i++) {
      Cronet_HttpHeaderPtr header = Cronet_UrlResponseInfo_all_headers_list_at(info, i);
      const char *key = Cronet_HttpHeader_name_get(header);
      const char *value = Cronet_HttpHeader_value_get(header);

      PyObject *py_key = PyUnicode_FromStringAndSize(key, strlen(key));
      PyObject *py_value = PyUnicode_FromStringAndSize(value, strlen(value));
      PyObject *py_header = PyTuple_New((Py_ssize_t)2);
      PyTuple_SetItem(py_header, (Py_ssize_t)0, py_key);
      PyTuple_SetItem(py_header, (Py_ssize_t)1, py_value);
      PyList_Append(headers, py_header);
      //PyDict_SetItemString(headers, key, item);
  }
  PyObject_CallMethod(ctx->py_callback, "on_redirect_received",
                      "ssiO", url, newLocationUrl, status_code, headers);
  PyGILState_Release(gstate);

  if (ctx->allow_redirects) {
    Cronet_UrlRequest_FollowRedirect(request);
  } else {
    Cronet_UrlRequest_Cancel(request);
  }
}


void on_response_started(Cronet_UrlRequestCallbackPtr callback,
                         Cronet_UrlRequestPtr request,
                         Cronet_UrlResponseInfoPtr info) {
  RequestContext *ctx = (RequestContext*)Cronet_UrlRequest_GetClientContext(request);
  int status_code = Cronet_UrlResponseInfo_http_status_code_get(info);
  int headers_size = Cronet_UrlResponseInfo_all_headers_list_size(info);
  const char *url = Cronet_UrlResponseInfo_url_get(info);
  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();
  //PyObject *headers = PyDict_New();
  PyObject *headers = PyList_New((Py_ssize_t)0);
  for (int i=0; i < headers_size; i++) {
      Cronet_HttpHeaderPtr header = Cronet_UrlResponseInfo_all_headers_list_at(info, i);
      const char *key = Cronet_HttpHeader_name_get(header);
      const char *value = Cronet_HttpHeader_value_get(header);

      PyObject *py_key = PyUnicode_FromStringAndSize(key, strlen(key));
      PyObject *py_value = PyUnicode_FromStringAndSize(value, strlen(value));
      PyObject *py_header = PyTuple_New((Py_ssize_t)2);
      PyTuple_SetItem(py_header, (Py_ssize_t)0, py_key);
      PyTuple_SetItem(py_header, (Py_ssize_t)1, py_value);
      PyList_Append(headers, py_header);
      //PyDict_SetItemString(headers, key, item);
  }
  PyObject_CallMethod(ctx->py_callback, "on_response_started", "siO",
                      url, status_code, headers);
  Py_DECREF(headers);
  PyGILState_Release(gstate);
  Cronet_BufferPtr buffer = Cronet_Buffer_Create();
  Cronet_Buffer_InitWithAlloc(buffer, 32 * 1024);
  Cronet_UrlRequest_Read(request, buffer);
}


void on_read_completed(Cronet_UrlRequestCallbackPtr callback,
                       Cronet_UrlRequestPtr request,
                       Cronet_UrlResponseInfoPtr info, Cronet_BufferPtr buffer,
                       uint64_t bytesRead) {
  RequestContext *ctx = (RequestContext*)Cronet_UrlRequest_GetClientContext(request);
  const char *buf_data = (const char*)Cronet_Buffer_GetData(buffer);
  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();
  PyObject *data = PyBytes_FromStringAndSize(buf_data, bytesRead);
  PyObject_CallMethod(ctx->py_callback, "on_read_completed", "O", data);
  Py_DECREF(data);
  PyGILState_Release(gstate);
  Cronet_UrlRequest_Read(request, buffer);
}


void on_succeeded(Cronet_UrlRequestCallbackPtr callback,
                  Cronet_UrlRequestPtr request,
                  Cronet_UrlResponseInfoPtr info) {
  RequestContext *ctx = (RequestContext*)Cronet_UrlRequest_GetClientContext(request);

  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();
  PyObject_CallMethod(ctx->py_callback, "on_succeeded", NULL);
  RequestContext_destroy(ctx);
  PyGILState_Release(gstate);

  Cronet_UrlRequest_Destroy(request);
}


void on_failed(Cronet_UrlRequestCallbackPtr callback,
               Cronet_UrlRequestPtr request, Cronet_UrlResponseInfoPtr info,
               Cronet_ErrorPtr error) {

  RequestContext *ctx = (RequestContext*)Cronet_UrlRequest_GetClientContext(request);
  const char *error_msg = Cronet_Error_message_get(error);

  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();
  PyObject_CallMethod(ctx->py_callback, "on_failed", "s", error_msg);
  RequestContext_destroy(ctx);
  PyGILState_Release(gstate);

  Cronet_UrlRequest_Destroy(request);
}


void on_canceled(Cronet_UrlRequestCallbackPtr callback,
                 Cronet_UrlRequestPtr request, Cronet_UrlResponseInfoPtr info) {
  RequestContext *ctx = (RequestContext*)Cronet_UrlRequest_GetClientContext(request);

  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();
  PyObject_CallMethod(ctx->py_callback, "on_canceled", NULL);
  RequestContext_destroy(ctx);
  PyGILState_Release(gstate);

  Cronet_UrlRequest_Destroy(request);
}


int64_t request_content_length(Cronet_UploadDataProviderPtr self)
{
    UploadProviderContext *ctx =
      (UploadProviderContext *)Cronet_UploadDataProvider_GetClientContext(self);
    return ctx->upload_size;
}


void request_content_read(Cronet_UploadDataProviderPtr self,
                          Cronet_UploadDataSinkPtr upload_data_sink,
                          Cronet_BufferPtr buffer)
{
    size_t buffer_size = Cronet_Buffer_GetSize(buffer);
    UploadProviderContext *ctx =
      (UploadProviderContext *)Cronet_UploadDataProvider_GetClientContext(self);

    size_t offset = ctx->upload_bytes_read;
    size_t rem = ctx->upload_size - ctx->upload_bytes_read;
    size_t size = buffer_size >= rem ? rem : buffer_size;
    memcpy(Cronet_Buffer_GetData(buffer), ctx->content + offset, size);
    ctx->upload_bytes_read += size;
    Cronet_UploadDataSink_OnReadSucceeded(upload_data_sink, size, false);
}


void request_content_rewind(Cronet_UploadDataProviderPtr self,
                            Cronet_UploadDataSinkPtr upload_data_sink)
{
}


void request_content_close(Cronet_UploadDataProviderPtr self)
{
  free(Cronet_UploadDataProvider_GetClientContext(self));
}


typedef struct {
  PyObject_HEAD
  Cronet_EnginePtr engine;
  Cronet_ExecutorPtr executor;
  ExecutorContext executor_context;
  pthread_t executor_thread;
} CronetEngineObject;


static void CronetEngine_dealloc(CronetEngineObject *self) {
  self->executor_context.should_stop = true;
  pthread_join(self->executor_thread, NULL);
  runnables_destroy(self->executor_context.runnables);
  pthread_cond_destroy(&self->executor_context.new_item);
  pthread_mutex_destroy(&self->executor_context.mutex);
  Cronet_Executor_Destroy(self->executor);
  Cronet_Engine_Shutdown(self->engine);
  Cronet_Engine_Destroy(self->engine);
  Py_TYPE(self)->tp_free((PyObject *)self);
}


static int CronetEngine_init(CronetEngineObject *self, PyObject *args, PyObject *kwds) {
  self->engine = Cronet_Engine_Create();
  bool engine_running = false;
  Cronet_EngineParamsPtr engine_params = Cronet_EngineParams_Create();
  if (!self->engine || !engine_params) {
    PyErr_SetString(PyExc_RuntimeError, "Could not create engine");
    goto fail;
  }
  PyObject *py_proxy_rules = NULL;
  if (!PyArg_ParseTuple(args, "O", &py_proxy_rules)) {
    goto fail;
  }
  if (!Py_IsNone(py_proxy_rules)) {
    const char *proxy_rules = PyUnicode_AsUTF8(py_proxy_rules);
    if (!proxy_rules) {
      goto fail;
    }
    Cronet_EngineParams_proxy_rules_set(engine_params, proxy_rules);
    LOG(proxy_rules);
  }
  Cronet_EngineParams_http_cache_mode_set(
      engine_params, Cronet_EngineParams_HTTP_CACHE_MODE_DISABLED);
  Cronet_EngineParams_enable_quic_set(engine_params, false);
  Cronet_EngineParams_user_agent_set(engine_params, "python-cronet");

  Cronet_RESULT res = Cronet_Engine_StartWithParams(self->engine, engine_params);
  if (res < 0) {
    PyErr_Format(PyExc_RuntimeError, "Could not start engine(%d)", res);
    goto fail;
  }
  engine_running = true;
  Cronet_EngineParams_Destroy(engine_params);
  engine_params = NULL;
  self->executor = Cronet_Executor_CreateWith(&execute_runnable);
  if (!self->executor) {
    PyErr_SetString(PyExc_RuntimeError, "Could not create executor");
    goto fail;
  }
  Runnables* runnables = (Runnables*)malloc(sizeof(Runnables));
  runnables_init(runnables);
  if (!runnables) {
    abort();
  }
  self->executor_context = (ExecutorContext){
      .runnables = runnables,
      .mutex = PTHREAD_MUTEX_INITIALIZER,
      .new_item = PTHREAD_COND_INITIALIZER,
      .should_stop = false,
  };
  Cronet_Executor_SetClientContext(self->executor,
                                   (void *)&self->executor_context);
  pthread_t executor_t;
  if (pthread_create(&executor_t, NULL, process_requests,
                     (void *)&self->executor_context) != 0) {
    PyErr_SetString(PyExc_RuntimeError, "Could not start executor thread");
    goto fail;
  }
  self->executor_thread = executor_t;
  LOG("Started cronet engine");
  return 0;

fail:
  if (engine_running) {
    Cronet_Engine_Shutdown(self->engine);
  }
  if (self->executor) {
    Cronet_Executor_Destroy(self->executor);
  }
  if (engine_params) {
    Cronet_EngineParams_Destroy(engine_params);
  }
  if (self->engine) {
    Cronet_Engine_Destroy(self->engine);
  }
  return -1;
}


static PyObject *CronetEngine_request(CronetEngineObject *self, PyObject *args) {
  PyObject *py_callback = NULL;
  if (!PyArg_ParseTuple(args, "O", &py_callback)) {
    return NULL;
  }
  PyObject *py_request = PyObject_GetAttrString(py_callback, "request");
  if (!py_request) {
    return NULL;
  }
  PyObject* url = PyObject_GetAttrString(py_request, "url");
  if (!url) {
    return NULL;
  }
  PyObject* method = PyObject_GetAttrString(py_request, "method");
  if (!method) {
    return NULL;
  }
  PyObject* content = PyObject_GetAttrString(py_request, "content");
  if (!content) {
    return NULL;
  }
  PyObject* headers = PyObject_GetAttrString(py_request, "headers");
  if (!headers) {
    return NULL;
  }

  const char *c_url = PyUnicode_AsUTF8(url);
  if (!c_url) {
    return NULL;
  }
  const char *c_method = PyUnicode_AsUTF8(method);
  if (!c_method) {
    return NULL;
  }
  char *c_content = NULL;
  if (!Py_IsNone(content)) {
    c_content = PyBytes_AsString(content);
    if (!c_content) {
      return NULL;
    }
  }

  Cronet_UrlRequestPtr request = Cronet_UrlRequest_Create();
  Cronet_UrlRequestParamsPtr request_params = Cronet_UrlRequestParams_Create();
  Cronet_UrlRequestParams_http_method_set(request_params, c_method);

  if (!Py_IsNone(headers)) {
    PyObject *items = PyDict_Items(headers);
    if (!items) {
      return NULL;
    }
    Py_ssize_t size = PyList_Size(items);
    for (Py_ssize_t i = 0; i < size; i++) {
      PyObject *item = PyList_GetItem(items, i);
      if (!item) {
        return NULL;
      }
      PyObject *key_obj = PyTuple_GetItem(item, 0);
      if (!key_obj) {
        return NULL;
      }
      PyObject* value_obj = PyTuple_GetItem(item, 1);
      if (!value_obj) {
        return NULL;
      }
      const char* key = PyUnicode_AsUTF8(key_obj);
      if (!key) {
        return NULL;
      }
      const char* value = PyUnicode_AsUTF8(value_obj);
      if (!value) {
        return NULL;
      }
      Cronet_HttpHeaderPtr request_header = Cronet_HttpHeader_Create();
      Cronet_HttpHeader_name_set(request_header, key);
      Cronet_HttpHeader_value_set(request_header, value);
      Cronet_UrlRequestParams_request_headers_add(request_params, request_header);
    }
  }

  PyObject *py_allow_redirects = PyObject_GetAttrString(py_request, "allow_redirects");
  if (!py_allow_redirects) {
    return NULL;
  }
  int allow_redirects = PyObject_IsTrue(py_allow_redirects);
  if (allow_redirects == -1) {
    return NULL;
  }

  Py_INCREF(py_callback);
  Cronet_UrlRequestCallbackPtr callback = Cronet_UrlRequestCallback_CreateWith(
      &on_redirect_received, &on_response_started, &on_read_completed,
      &on_succeeded, &on_failed, &on_canceled);

  RequestContext *ctx = (RequestContext*)malloc(sizeof(RequestContext));
  if (!ctx) {
    abort();
  }
  ctx->callback = NULL;
  ctx->allow_redirects = (bool)allow_redirects;
  ctx->py_callback = py_callback;
  if (c_content) {
    UploadProviderContext *upload_ctx =
      (UploadProviderContext*)malloc(sizeof(UploadProviderContext));
    if (!upload_ctx) {
      abort();
    }
    upload_ctx->content = c_content;
    upload_ctx->upload_size = strlen(c_content);
    upload_ctx->upload_bytes_read = 0;
    Cronet_UploadDataProviderPtr data_provider =
      Cronet_UploadDataProvider_CreateWith(&request_content_length,
                                           &request_content_read,
                                           &request_content_rewind,
                                           &request_content_close);

    Cronet_UploadDataProvider_SetClientContext(data_provider, (void*)upload_ctx);
    Cronet_UrlRequestParams_upload_data_provider_set(request_params, data_provider);
  }

  Cronet_UrlRequest_SetClientContext(request, (void*)ctx);
  Cronet_UrlRequest_InitWithParams(request, self->engine, c_url, request_params,
                                   callback, self->executor);
  Cronet_UrlRequestParams_Destroy(request_params);
  PyObject *capsule = PyCapsule_New(request, NULL, NULL);
  if (!capsule) {
    Py_DECREF(py_callback);
    RequestContext_destroy(ctx);
    Cronet_UrlRequest_Destroy(request);
    return NULL;
  }
  Cronet_UrlRequest_Start(request);
  return capsule;
}


static PyObject *CronetEngine_cancel(CronetEngineObject *self, PyObject *args) {
  PyObject *capsule = NULL;
  if (!PyArg_ParseTuple(args, "O", &capsule)) {
    return NULL;
  }
  void *request = PyCapsule_GetPointer(capsule, NULL);
  if (!request) {
    return NULL;
  }
  Cronet_UrlRequest_Cancel((Cronet_UrlRequestPtr)request);
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
