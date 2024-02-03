#ifndef URL_REQUEST_CALLBACK_H_
#define URL_REQUEST_CALLBACK_H_

#include <future>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <tuple>

#include "cronet_c.h"

class UrlRequestCallback {
 public:
  UrlRequestCallback();
  ~UrlRequestCallback();

  // Gets Cronet_UrlRequestCallbackPtr implemented by |this|.
  Cronet_UrlRequestCallbackPtr GetUrlRequestCallback();

  // Waits until request is done.
  void WaitForDone() { is_done_.wait(); }

  // Returns error message if OnFailed callback is invoked.
  std::string last_error_message() const { return last_error_message_; }
  // Returns string representation of the received response.
  std::string response_as_string() const { return response_as_string_; }
  int32_t status_code;
  std::vector<std::tuple<std::string, std::string> > headers;

 protected:
  void OnRedirectReceived(Cronet_UrlRequestPtr request,
                          Cronet_UrlResponseInfoPtr info,
                          Cronet_String newLocationUrl);

  void OnResponseStarted(Cronet_UrlRequestPtr request,
                         Cronet_UrlResponseInfoPtr info);

  void OnReadCompleted(Cronet_UrlRequestPtr request,
                       Cronet_UrlResponseInfoPtr info,
                       Cronet_BufferPtr buffer,
                       uint64_t bytes_read);

  void OnSucceeded(Cronet_UrlRequestPtr request,
                   Cronet_UrlResponseInfoPtr info);

  void OnFailed(Cronet_UrlRequestPtr request,
                Cronet_UrlResponseInfoPtr info,
                Cronet_ErrorPtr error);

  void OnCanceled(Cronet_UrlRequestPtr request, Cronet_UrlResponseInfoPtr info);

  void SignalDone(bool success) { done_with_success_.set_value(success); }

  static UrlRequestCallback* GetThis(Cronet_UrlRequestCallbackPtr self);

  // Implementation of Cronet_UrlRequestCallback methods.
  static void OnRedirectReceived(Cronet_UrlRequestCallbackPtr self,
                                 Cronet_UrlRequestPtr request,
                                 Cronet_UrlResponseInfoPtr info,
                                 Cronet_String newLocationUrl);

  static void OnResponseStarted(Cronet_UrlRequestCallbackPtr self,
                                Cronet_UrlRequestPtr request,
                                Cronet_UrlResponseInfoPtr info);

  static void OnReadCompleted(Cronet_UrlRequestCallbackPtr self,
                              Cronet_UrlRequestPtr request,
                              Cronet_UrlResponseInfoPtr info,
                              Cronet_BufferPtr buffer,
                              uint64_t bytesRead);

  static void OnSucceeded(Cronet_UrlRequestCallbackPtr self,
                          Cronet_UrlRequestPtr request,
                          Cronet_UrlResponseInfoPtr info);

  static void OnFailed(Cronet_UrlRequestCallbackPtr self,
                       Cronet_UrlRequestPtr request,
                       Cronet_UrlResponseInfoPtr info,
                       Cronet_ErrorPtr error);

  static void OnCanceled(Cronet_UrlRequestCallbackPtr self,
                         Cronet_UrlRequestPtr request,
                         Cronet_UrlResponseInfoPtr info);

  // Error message copied from |error| if OnFailed callback is invoked.
  std::string last_error_message_;
  // Accumulated string representation of the received response.
  std::string response_as_string_;
  // Promise that is set when request is done.
  std::promise<bool> done_with_success_;
  // Future that is signalled when request is done.
  std::future<bool> is_done_ = done_with_success_.get_future();

  Cronet_UrlRequestCallbackPtr const callback_;
};

#endif  // URL_REQUEST_CALLBACK_H_
