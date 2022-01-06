#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include <wx/image.h>

wxDECLARE_EVENT(okEVT_PREVIOUS_TAB, wxCommandEvent);
wxDECLARE_EVENT(okEVT_NEXT_TAB, wxCommandEvent);
wxDECLARE_EVENT(okEVT_PREVIOUS_PAGE, wxCommandEvent);
wxDECLARE_EVENT(okEVT_NEXT_PAGE, wxCommandEvent);

wxDECLARE_EVENT(okEVT_SETTINGS_CHANGED, wxCommandEvent);
wxDECLARE_EVENT(okEVT_GAME_CHANGED, wxCommandEvent);

/** Should only be emitted by `okTab`/`okTabCanvas`.
 *
 * Reasonable responses include:
 * - repainting the window/widget
 * - updating any other renderers, such as SHM or SteamVR.
 */
wxDECLARE_EVENT(okEVT_TAB_PIXELS_CHANGED, wxCommandEvent);

/** The contents of a tab has been fully replaced.
 *
 * For example, `DCSMissionTab` should emit this when the current mission
 * changes.
 * 
 * Reasonable responses for okTab and okTabCanvas include:
 * - resetting back to the first page
 * - emitting `okEVT_TAB_PIXELS_MODIFIED`
 */
wxDECLARE_EVENT(okEVT_TAB_FULLY_REPLACED, wxCommandEvent);

/** A page has been modified, rather than being replaced.
 *
 * For example, `DCSRadioLogTab` will emit this when a new entry is added to
 * the current page.
 *
 * Reasonable responses for okTab and okTabCanvas include emitting
 * `okEVT_TAB_PIXELS_MODIFIED`, but not changing the page number.
 */
wxDECLARE_EVENT(okEVT_TAB_PAGE_MODIFIED, wxCommandEvent);

/** A page has been appended, but existing pages have not been modified.
 *
 * If rendered page content includes the total number of pages (e.g.
 * 'Page x of y'), `okEVT_TAB_PAGE_MODIFIED` should also be emitted so that the
 * current page is updated.
 *
 * Reasonable responses include making the new page the current page.
 */
wxDECLARE_EVENT(okEVT_TAB_PAGE_APPENDED, wxCommandEvent);
