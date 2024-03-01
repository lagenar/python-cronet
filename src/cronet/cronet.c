#define PY_SSIZE_T_CLEAN
#include <pthread.h>
#include <stdbool.h>

#include "Python.h"
#include "cronet_c.h"

/* saves the cronet runnable to execute next for a single executor 
   cronet runables are the atomic steps of a request.
*/
typedef struct {
  Cronet_RunnablePtr runnable;
  pthread_mutex_t runnable_mutex;
  pthread_cond_t cond;
  pthread_mutex_t cond_mutex;
} ExecutorRunnable;


typedef struct {
  PyObject_HEAD 
  Cronet_EnginePtr engine;
  Cronet_ExecutorPtr executor;
  ExecutorRunnable executor_runnable;
} CronetObject;


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
  while (true) {
    pthread_cond_wait(&run->cond, &run->cond_mutex);
    pthread_mutex_lock(&run->runnable_mutex);
    if (run->runnable != NULL) {
      Cronet_Runnable_Run(run->runnable);
      Cronet_Runnable_Destroy(run->runnable);
      run->runnable = NULL;
    }
    pthread_mutex_unlock(&run->runnable_mutex);
  }
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
  PyObject_CallMethod(py_request, "on_response_started", "iO", status_code, headers);
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


static void Cronet_dealloc(CronetObject *self) {
  Cronet_Executor_Destroy(self->executor);
  Cronet_Engine_Shutdown(self->engine);
  Cronet_Engine_Destroy(self->engine);
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static int Cronet_init(CronetObject *self, PyObject *args, PyObject *kwds) {
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
  };
  Cronet_Executor_SetClientContext(self->executor,
                                   (void *)&self->executor_runnable);
  pthread_t executor_t;
  if (pthread_create(&executor_t, NULL, process_requests,
                     (void *)&self->executor_runnable) != 0) {
    perror("pthread_create failed");
    return 1;
  }

  return 0;
}

static PyObject *Cronet_request(CronetObject *self, PyObject *args) {
  /* TODO: add validation */
  PyObject *py_request = NULL;
  if (!PyArg_ParseTuple(args, "O", &py_request)) {
    perror("failed to parse arguments\n");
    Py_RETURN_NONE;
  }
  Py_IncRef(py_request);

  PyObject* url = PyObject_GetAttrString(py_request, "url");
  PyObject* method = PyObject_GetAttrString(py_request, "method");
  const char *c_url = PyUnicode_AsUTF8(url);
  const char *c_method = PyUnicode_AsUTF8(method);

  Cronet_UrlRequestPtr request = Cronet_UrlRequest_Create();
  Cronet_UrlRequestParamsPtr request_params = Cronet_UrlRequestParams_Create();
  Cronet_UrlRequestParams_http_method_set(request_params, c_method);

  Cronet_UrlRequestCallbackPtr callback = Cronet_UrlRequestCallback_CreateWith(
      &on_redirect_received, &on_response_started, &on_read_completed,
      &on_succeeded, &on_failed, &on_canceled);
  
  Py_INCREF(py_request);
  Cronet_UrlRequest_SetClientContext(request, (void *)py_request);

  Cronet_UrlRequest_InitWithParams(request, self->engine, c_url, request_params,
                                   callback, self->executor);

  Cronet_UrlRequestParams_Destroy(request_params);

  Cronet_UrlRequest_Start(request);
  Py_RETURN_NONE;
}

static PyMethodDef Cronet_methods[] = {
    {"request", (PyCFunction)Cronet_request, METH_VARARGS,
     "Run a request"},
    {NULL} /* Sentinel */
};

static PyTypeObject CronetType = {
    .ob_base = PyVarObject_HEAD_INIT(NULL, 0).tp_name = "cronet.Cronet",
    .tp_doc = PyDoc_STR("Cronet engine"),
    .tp_basicsize = sizeof(CronetObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = PyType_GenericNew,
    .tp_init = (initproc)Cronet_init,
    .tp_dealloc = (destructor)Cronet_dealloc,
    .tp_methods = Cronet_methods,
};

static PyModuleDef cronetmodule = {
    .m_base = PyModuleDef_HEAD_INIT,
    .m_name = "cronet",
    .m_doc = "",
    .m_size = -1,
};

PyMODINIT_FUNC PyInit_cronet(void) {
  PyObject *m;
  if (PyType_Ready(&CronetType) < 0)
    return NULL;

  m = PyModule_Create(&cronetmodule);
  if (m == NULL)
    return NULL;

  Py_INCREF(&CronetType);
  if (PyModule_AddObject(m, "Cronet", (PyObject *)&CronetType) < 0) {
    Py_DECREF(&CronetType);
    Py_DECREF(m);
    return NULL;
  }

  return m;
}
