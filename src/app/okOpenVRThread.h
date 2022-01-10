#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/thread.h>

#include <memory>
#include <vector>

class okOpenVRThread final : public wxThread {
 private:
  class Impl;
  std::unique_ptr<Impl> p;

 public:
  okOpenVRThread();
  ~okOpenVRThread();

 protected:
  virtual ExitCode Entry() override;

 private:
  void Tick();
};
