#pragma once

#include "okTabsList.h"

class wxListEvent;
class wxListView;

class okTabsList::SettingsUI final : public wxPanel {
  public:
    SettingsUI(wxWindow* parent, const std::shared_ptr<SharedState>&);
    virtual ~SettingsUI();
  private:
    std::shared_ptr<SharedState> s;
    wxListView* mList = nullptr;

    void OnAddTab(wxCommandEvent&);
    void OnRemoveTab(wxCommandEvent&);

    void OnMoveTabUp(wxCommandEvent&);
    void OnMoveTabDown(wxCommandEvent&);

    enum class Direction {
      UP,
      DOWN,
    };
    void MoveTab(Direction);

    void InsertTab(const std::shared_ptr<OpenKneeboard::Tab>&);
    void InsertFolderTab();
};
