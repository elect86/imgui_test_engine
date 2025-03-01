// dear imgui test engine
// (context when a running test + end user automation API)
// This is the main (if not only) interface that your Tests will be using.

#pragma once

#include "imgui.h"
#include "imgui_internal.h"     // ImGuiAxis, ImGuiItemStatusFlags, ImGuiInputSource, ImGuiWindow
#include "imgui_te_engine.h"    // ImGuiTestStatus, ImGuiTestRunFlags, ImGuiTestActiveFunc, ImGuiTestItemInfo, ImGuiTestLogFlags

/*

Index of this file:
// [SECTION] Header mess, warnings
// [SECTION] External forward declaration
// [SECTION] ImGuiTestRef
// [SECTION] Helper keys
// [SECTION] ImGuiTestContext related Flags/Enumerations
// [SECTION] ImGuiTestGenericVars, ImGuiTestGenericItemStatus
// [SECTION] ImGuiTestContext
// [SECTION] Debugging macros
// [SECTION] IM_CHECK macros

*/

//-------------------------------------------------------------------------
// [SECTION] Header mess, warnings
//-------------------------------------------------------------------------

// Undo some of the damage done by <windows.h>
#ifdef Yield
#undef Yield
#endif

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"                // warning: unknown warning group 'xxx'
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"                          // warning: unknown option after '#pragma GCC diagnostic' kind
#pragma GCC diagnostic ignored "-Wclass-memaccess"                  // [__GNUC__ >= 8] warning: 'memset/memcpy' clearing/writing an object of type 'xxxx' with no trivial copy-assignment; use assignment or value-initialization instead
#endif

//-------------------------------------------------------------------------
// [SECTION] Forward declaration
//-------------------------------------------------------------------------

// This file
typedef int ImGuiTestOpFlags;       // Flags: See ImGuiTestOpFlags_

// External: imgui
struct ImGuiDockNode;
struct ImGuiTabBar;
struct ImGuiWindow;

// External: test engine
struct ImGuiTest;                   // A test registered with IM_REGISTER_TEST()
struct ImGuiTestEngine;             // Test Engine Instance (opaque)
struct ImGuiTestEngineIO;           // Test Engine IO structure (configuration flags, state)
struct ImGuiTestItemInfo;           // Information gathered about an item: label, status, bounding box etc.
struct ImGuiTestItemList;           // Result of an GatherItems() query
struct ImGuiTestInputs;             // Test Engine Simulated Inputs structure (opaque)
struct ImGuiTestGatherTask;         // Test Engine task for scanning/finding items
struct ImGuiCaptureArgs;            // Parameters for ctx->CaptureXXX functions
enum ImGuiTestVerboseLevel : int;

//-------------------------------------------------------------------------
// [SECTION] ImGuiTestRef
//-------------------------------------------------------------------------

// Weak reference to an Item/Window given an hashed ID _or_ a string path ID.
// This is most often passed as argument to function and generally has a very short lifetime.
// Documentation: https://github.com/ocornut/imgui_test_engine/wiki/Named-References
// (SUGGESTION: add those constructors to "VA Step Filter" (Visual Assist) or a .natstepfilter file (Visual Studio) so they are skipped by F11 (StepInto)
struct IMGUI_API ImGuiTestRef
{
    ImGuiID         ID;             // Pre-hashed ID
    const char*     Path;           // Relative or absolute path (string pointed to, not owned, as our lifetime is very short)

    ImGuiTestRef()                  { ID = 0; Path = NULL; }
    ImGuiTestRef(ImGuiID id)        { ID = id; Path = NULL; }
    ImGuiTestRef(const char* path)  { ID = 0; Path = path; }
    bool IsEmpty() const            { return ID == 0 && (Path == NULL || Path[0] == 0); }
};

// Debug helper to output a string showing the Path, ID or Debug Label based on what is available (some items only have ID as we couldn't find/store a Path)
// (The size is arbitrary, this is only used for logging info the user/debugger)
struct IMGUI_API ImGuiTestRefDesc
{
    char            Buf[80];

    const char* c_str()             { return Buf; }
    ImGuiTestRefDesc(const ImGuiTestRef& ref, const ImGuiTestItemInfo* item);
};

//-------------------------------------------------------------------------
// [SECTION] ImGuiTestContext related Flags/Enumerations
//-------------------------------------------------------------------------

// Named actions. Generally you will call the named helpers e.g. ItemClick(). This is used by shared/low-level functions such as ItemAction().
enum ImGuiTestAction
{
    ImGuiTestAction_Unknown = 0,
    ImGuiTestAction_Hover,          // Move mouse
    ImGuiTestAction_Click,          // Move mouse and click
    ImGuiTestAction_DoubleClick,    // Move mouse and double-click
    ImGuiTestAction_Check,          // Check item if unchecked (Checkbox, MenuItem or any widget reporting ImGuiItemStatusFlags_Checkable)
    ImGuiTestAction_Uncheck,        // Uncheck item if checked
    ImGuiTestAction_Open,           // Open item if closed (TreeNode, BeginMenu or any widget reporting ImGuiItemStatusFlags_Openable)
    ImGuiTestAction_Close,          // Close item if opened
    ImGuiTestAction_Input,          // Start text inputing into a field (e.g. CTRL+Click on Drags/Slider, click on InputText etc.)
    ImGuiTestAction_NavActivate,    // Activate item with navigation
    ImGuiTestAction_COUNT
};

// Generic flags for many ImGuiTestContext functions
enum ImGuiTestOpFlags_
{
    ImGuiTestOpFlags_None               = 0,
    ImGuiTestOpFlags_NoCheckHoveredId   = 1 << 1,   // Don't check for HoveredId after aiming for a widget. A few situations may want this: while e.g. dragging or another items prevents hovering, or for items that don't use ItemHoverable()
    ImGuiTestOpFlags_NoError            = 1 << 2,   // Don't abort/error e.g. if the item cannot be found or the operation doesn't succeed.
    ImGuiTestOpFlags_NoFocusWindow      = 1 << 3,   // Don't focus window when aiming at an item
    ImGuiTestOpFlags_NoAutoUncollapse   = 1 << 4,   // Disable automatically uncollapsing windows (useful when specifically testing Collapsing behaviors)
    ImGuiTestOpFlags_NoAutoOpenFullPath = 1 << 5,   // Disable automatically opening intermediaries (e.g. ItemClick("Hello/OK") will automatically first open "Hello" if "OK" isn't found. Only works if ref is a string path.
    ImGuiTestOpFlags_IsSecondAttempt    = 1 << 6,   // Used by recursing functions to indicate a second attempt
    ImGuiTestOpFlags_MoveToEdgeL        = 1 << 7,   // Simple Dumb aiming helpers to test widget that care about clicking position. May need to replace will better functionalities.
    ImGuiTestOpFlags_MoveToEdgeR        = 1 << 8,
    ImGuiTestOpFlags_MoveToEdgeU        = 1 << 9,
    ImGuiTestOpFlags_MoveToEdgeD        = 1 << 10,
};

// Advanced filtering for ItemActionAll()
struct IMGUI_API ImGuiTestActionFilter
{
    int                     MaxDepth;
    int                     MaxPasses;
    const int*              MaxItemCountPerDepth;
    ImGuiItemStatusFlags    RequireAllStatusFlags;
    ImGuiItemStatusFlags    RequireAnyStatusFlags;

    ImGuiTestActionFilter() { MaxDepth = -1; MaxPasses = -1; MaxItemCountPerDepth = NULL; RequireAllStatusFlags = RequireAnyStatusFlags = 0; }
};

//-------------------------------------------------------------------------
// [SECTION] ImGuiTestGenericVars, ImGuiTestGenericItemStatus
//-------------------------------------------------------------------------

// Helper struct to store various query-able state of an item.
// This facilitate interactions between GuiFunc and TestFunc, since those state are frequently used.
struct IMGUI_API ImGuiTestGenericItemStatus
{
    int     Ret;                    // return value
    int     Hovered;                // result of IsItemHovered()
    int     Active;                 // result of IsItemActive()
    int     Focused;                // result of IsItemFocused()
    int     Clicked;                // result of IsItemClicked()
    int     Visible;                // result of IsItemVisible()
    int     Edited;                 // result of IsItemEdited()
    int     Activated;              // result of IsItemActivated()
    int     Deactivated;            // result of IsItemDeactivated()
    int     DeactivatedAfterEdit;   // .. of IsItemDeactivatedAfterEdit()

    ImGuiTestGenericItemStatus()        { Clear(); }
    void Clear()                        { memset(this, 0, sizeof(*this)); }
    void QuerySet(bool ret_val = false) { Clear(); QueryInc(ret_val); }
    void QueryInc(bool ret_val = false) { Ret += ret_val; Hovered += ImGui::IsItemHovered(); Active += ImGui::IsItemActive(); Focused += ImGui::IsItemFocused(); Clicked += ImGui::IsItemClicked(); Visible += ImGui::IsItemVisible(); Edited += ImGui::IsItemEdited(); Activated += ImGui::IsItemActivated(); Deactivated += ImGui::IsItemDeactivated(); DeactivatedAfterEdit += ImGui::IsItemDeactivatedAfterEdit(); }
};

// Generic structure with various storage fields.
// This is useful for tests to quickly share data between GuiFunc and TestFunc without creating custom data structure.
// If those fields are not enough: using test->SetVarsDataType<>() + ctx->GetVars<>() it is possible to store custom data on the stack.
struct IMGUI_API ImGuiTestGenericVars
{
    // Generic storage with a bit of semantic to make user/test code look neater
    int                     Step;
    int                     Count;
    ImGuiID                 DockId;
    ImGuiWindowFlags        WindowFlags;
    ImGuiTableFlags         TableFlags;
    ImGuiTestGenericItemStatus  Status;
    bool                    ShowWindows;
    bool                    UseClipper;
    bool                    UseViewports;
    float                   Width;
    ImVec2                  Pos;
    ImVec2                  Size;
    ImVec2                  Pivot;
    ImVec4                  Color1, Color2;

    // Generic unnamed storage
    int                     Int1, Int2, IntArray[10];
    float                   Float1, Float2, FloatArray[10];
    bool                    Bool1, Bool2, BoolArray[10];
    ImGuiID                 Id, IdArray[10];
    char                    Str1[256], Str2[256];

    ImGuiTestGenericVars()  { Clear(); }
    void Clear()            { memset(this, 0, sizeof(*this)); }
};

//-------------------------------------------------------------------------
// [SECTION] ImGuiTestContext
// Context for a running ImGuiTest
// This is the interface that most tests will interact with.
//-------------------------------------------------------------------------

struct IMGUI_API ImGuiTestContext
{
    // User variables
    ImGuiTestGenericVars    GenericVars;
    void*                   UserVars = NULL;                        // Access using ctx->GetVars<Type>(). Setup with test->SetVarsDataType<>().

    // Public fields
    ImGuiContext*           UiContext = NULL;                       // UI context
    ImGuiTestEngineIO*      EngineIO = NULL;
    ImGuiTest*              Test = NULL;                            // Test being run
    ImGuiTestOpFlags        OpFlags = ImGuiTestOpFlags_None;        // Supported: ImGuiTestOpFlags_NoAutoUncollapse
    int                     PerfStressAmount = 0;                   // Convenience copy of engine->IO.PerfStressAmount
    int                     FrameCount = 0;                         // Test frame count (restarts from zero every time)
    int                     FirstTestFrameCount = 0;                // First frame where TestFunc is running (after warm-up frame). This is generally -1 or 0 depending on whether we have warm up enabled
    bool                    FirstGuiFrame = false;
    bool                    HasDock = false;                        // #ifdef IMGUI_HAS_DOCK expressed in an easier to test value
    ImGuiCaptureArgs*       CaptureArgs = NULL;                     // Capture settings used by ctx->Capture*() functions

    //-------------------------------------------------------------------------
    // [Internal Fields]
    //-------------------------------------------------------------------------

    ImGuiTestEngine*        Engine = NULL;
    ImGuiTestInputs*        Inputs = NULL;
    ImGuiTestGatherTask*    GatherTask = NULL;
    ImGuiTestRunFlags       RunFlags = ImGuiTestRunFlags_None;
    ImGuiTestActiveFunc     ActiveFunc = ImGuiTestActiveFunc_None;  // None/GuiFunc/TestFunc
    double                  RunningTime = 0.0;                      // Amount of wall clock time the Test has been running. Used by safety watchdog.
    ImU64                   BatchStartTime = 0;
    int                     ActionDepth = 0;                        // Nested depth of ctx-> function calls (used to decorate log)
    int                     CaptureCounter = 0;                     // Number of captures
    int                     ErrorCounter = 0;                       // Number of errors (generally this maxxes at 1 as most functions will early out)
    bool                    Abort = false;
    double                  PerfRefDt = -1.0;
    int                     PerfIterations = 400;                   // Number of frames for PerfCapture() measurements
    char                    RefStr[256] = { 0 };                    // Reference window/path for ID construction
    ImGuiID                 RefID = 0;
    ImGuiID                 RefWindowID = 0;                        // ID of a window that contains RefID item
    ImGuiInputSource        InputMode = ImGuiInputSource_Mouse;     // Prefer interacting with mouse/keyboard/gamepad
    ImVector<char>          Clipboard;                              // Private clipboard for the test instance
    ImVector<ImGuiWindow*>  ForeignWindowsToHide;
    ImGuiTestItemInfo       DummyItemInfoNull;                      // Storage for ItemInfoNull()
    bool                    CachedLinesPrintedToTTY = false;

    //-------------------------------------------------------------------------
    // Public API
    //-------------------------------------------------------------------------

    // Main control
    void        Finish();
    bool        IsError() const             { return Test->Status == ImGuiTestStatus_Error || Abort; }
    bool        IsFirstGuiFrame() const     { return FirstGuiFrame; }
    bool        IsFirstTestFrame() const    { return FrameCount == FirstTestFrameCount; }   // First frame where TestFunc is running (after warm-up frame).
    bool        IsGuiFuncOnly() const       { return (RunFlags & ImGuiTestRunFlags_GuiFuncOnly) != 0; }
    void        SetGuiFuncEnabled(bool v)   { if (v) RunFlags &= ~ImGuiTestRunFlags_GuiFuncDisable; else RunFlags |= ImGuiTestRunFlags_GuiFuncDisable; }
    void        RecoverFromUiContextErrors();
    template <typename T> T& GetVars()      { IM_ASSERT(UserVars != NULL); return *(T*)(UserVars); } // Campanion to using t->SetVarsDataType<>(). FIXME: Assert to compare sizes

    // Debug Control Flow
    bool        SuspendTestFunc(const char* file, int line);

    // Logging
    void        LogEx(ImGuiTestVerboseLevel level, ImGuiTestLogFlags flags, const char* fmt, ...) IM_FMTARGS(4);
    void        LogExV(ImGuiTestVerboseLevel level, ImGuiTestLogFlags flags, const char* fmt, va_list args) IM_FMTLIST(4);
    void        LogToTTY(ImGuiTestVerboseLevel level, const char* message, const char* message_end = NULL);
    void        LogToDebugger(ImGuiTestVerboseLevel level, const char* message);
    void        LogDebug(const char* fmt, ...)      IM_FMTARGS(2);  // ImGuiTestVerboseLevel_Debug or ImGuiTestVerboseLevel_Trace depending on context depth
    void        LogInfo(const char* fmt, ...)       IM_FMTARGS(2);  // ImGuiTestVerboseLevel_Info
    void        LogWarning(const char* fmt, ...)    IM_FMTARGS(2);  // ImGuiTestVerboseLevel_Warning
    void        LogError(const char* fmt, ...)      IM_FMTARGS(2);  // ImGuiTestVerboseLevel_Error
    void        LogBasicUiState();

    // Yield, Timing
    void        Yield(int count = 1);
    void        YieldUntil(int frame_count);
    void        Sleep(float time_in_second);            // Sleep for a given simulation time, unless in Fast mode
    void        SleepShort();                           // Standard short delay of io.ActionDelayShort (~0.15f), unless in Fast mode.
    void        SleepStandard();                        // Standard regular delay of io.ActionDelayStandard (~0.40f), unless in Fast mode.
    void        SleepNoSkip(float time_in_second, float framestep_in_second);

    // Base Reference
    // - ItemClick("Window/Button")                --> click "Window/Button"
    // - SetRef("Window"), ItemClick("Button")     --> click "Window/Button"
    // - SetRef("Window"), ItemClick("/Button")    --> click "Window/Button"
    // - SetRef("Window"), ItemClick("//Button")   --> click "/Button"
    // - SetRef("//$FOCUSED"), ItemClick("Button") --> click "Button" in focused window.
    // Takes multiple frames to complete if specified ref is an item id.
    void        SetRef(ImGuiTestRef ref);
    void        SetRef(ImGuiWindow* window); // Shortcut to SetRef(window->Name) which works for ChildWindow (see code)
    ImGuiTestRef GetRef();

    // Windows
    ImGuiTestItemInfo* WindowInfo(ImGuiTestRef window_ref, ImGuiTestOpFlags flags = ImGuiTestOpFlags_None);
    void        WindowClose(ImGuiTestRef window_ref);
    void        WindowCollapse(ImGuiTestRef window_ref, bool collapsed);
    void        WindowFocus(ImGuiTestRef window_ref);
    void        WindowMove(ImGuiTestRef window_ref, ImVec2 pos, ImVec2 pivot = ImVec2(0.0f, 0.0f), ImGuiTestOpFlags flags = ImGuiTestOpFlags_None);
    void        WindowResize(ImGuiTestRef window_ref, ImVec2 sz);
    bool        WindowTeleportToMakePosVisible(ImGuiTestRef window_ref, ImVec2 pos_in_window);
    bool        WindowBringToFront(ImGuiTestRef window_ref, ImGuiTestOpFlags flags = ImGuiTestOpFlags_None);
    ImGuiWindow*GetWindowByRef(ImGuiTestRef window_ref);

    // Popups
    void        PopupCloseOne();
    void        PopupCloseAll();

    // Get hash for a decorated ID Path.
    // Note: for windows you may use WindowInfo()
    ImGuiID     GetID(ImGuiTestRef ref);
    ImGuiID     GetID(ImGuiTestRef ref, ImGuiTestRef seed_ref);

    // Misc
    ImVec2      GetPosOnVoid();                                                     // Find a point that has no windows // FIXME-VIEWPORT: This needs a viewport
    ImVec2      GetWindowTitlebarPoint(ImGuiTestRef window_ref);                    // Return a clickable point on window title-bar (window tab for docked windows).
    ImVec2      GetMainMonitorWorkPos();                                            // Work pos and size of main viewport when viewports are disabled, or work pos and size of monitor containing main viewport when viewports are enabled.
    ImVec2      GetMainMonitorWorkSize();

    // Screenshot/Video Captures
    void        CaptureReset();                                                     // Reset state (use when doing multiple captures)
    void        CaptureSetExtension(const char* ext);                               // Set capture file format (otherwise for video this default to EngineIO->VideoCaptureExtension)
    bool        CaptureAddWindow(ImGuiTestRef ref);                                 // Add window to be captured (default to capture everything)
    void        CaptureScreenshotWindow(ImGuiTestRef ref, int capture_flags = 0);   // Trigger a screen capture of a single window (== CaptureAddWindow() + CaptureScreenshot())
    bool        CaptureScreenshot(int capture_flags = 0);                           // Trigger a screen capture
    bool        CaptureBeginVideo();                                                // Start a video capture
    bool        CaptureEndVideo();

    // Mouse inputs
    void        MouseMove(ImGuiTestRef ref, ImGuiTestOpFlags flags = ImGuiTestOpFlags_None);
    void        MouseMoveToPos(ImVec2 pos);
    void        MouseTeleportToPos(ImVec2 pos);
    void        MouseClick(ImGuiMouseButton button = 0);
    void        MouseClickMulti(ImGuiMouseButton button, int count);
    void        MouseDoubleClick(ImGuiMouseButton button = 0);
    void        MouseDown(ImGuiMouseButton button = 0);
    void        MouseUp(ImGuiMouseButton button = 0);
    void        MouseLiftDragThreshold(ImGuiMouseButton button = 0);
    void        MouseDragWithDelta(ImVec2 delta, ImGuiMouseButton button = 0);
    void        MouseWheel(ImVec2 delta);
    void        MouseWheelX(float dx) { MouseWheel(ImVec2(dx, 0.0f)); }
    void        MouseWheelY(float dy) { MouseWheel(ImVec2(0.0f, dy)); }
    void        MouseMoveToVoid();
    void        MouseClickOnVoid(ImGuiMouseButton button = 0);
    bool        FindExistingVoidPosOnViewport(ImGuiViewport* viewport, ImVec2* out);

    // Mouse inputs: Viewports
    // When using MouseMoveToPos() / MouseTeleportToPos() without referring to an item may need to set that up.
    void        MouseSetViewport(ImGuiWindow* window);
    void        MouseSetViewportID(ImGuiID viewport_id);

    // Keyboard inputs
    void        KeyDown(ImGuiKeyChord key_chord);
    void        KeyUp(ImGuiKeyChord key_chord);
    void        KeyPress(ImGuiKeyChord key_chord, int count = 1);
    void        KeyHold(ImGuiKeyChord key_chord, float time);
    void        KeyChars(const char* chars);                // Input characters
    void        KeyCharsAppend(const char* chars);          // Input characters at end of field
    void        KeyCharsAppendEnter(const char* chars);     // Input characters at end of field, press Enter
    void        KeyCharsReplace(const char* chars);         // Delete existing field then input characters
    void        KeyCharsReplaceEnter(const char* chars);    // Delete existing field then input characters, press Enter

    // Navigation inputs
    // FIXME: Need some redesign/refactoring:
    // - This was initially intended to: replace mouse action with keyboard/gamepad
    // - Abstract keyboard vs gamepad actions
    // However this is widely inconsistent and unfinished at this point.
    void        SetInputMode(ImGuiInputSource input_mode);  // ImGuiInputSource_Mouse or ImGuiInputSource_Nav. In nav mode, actions such as ItemClick or ItemInput are using nav facilities instead of Mouse.
    void        NavMoveTo(ImGuiTestRef ref);
    void        NavActivate();                              // Activate current selected item: activate button, tweak sliders/drags. Equivalent of pressing Space on keyboard, ImGuiKey_GamepadFaceUp on a gamepad.
    void        NavInput();                                 // Input into select item: input sliders/drags. Equivalent of pressing Enter on keyboard, ImGuiKey_GamepadFaceDown on a gamepad.

    // Scrolling
    void        ScrollTo(ImGuiTestRef ref, ImGuiAxis axis, float scroll_v, ImGuiTestOpFlags flags = ImGuiTestOpFlags_None);
    void        ScrollToX(ImGuiTestRef ref, float scroll_x) { ScrollTo(ref, ImGuiAxis_X, scroll_x); }
    void        ScrollToY(ImGuiTestRef ref, float scroll_y) { ScrollTo(ref, ImGuiAxis_Y, scroll_y); }
    void        ScrollToTop(ImGuiTestRef ref);
    void        ScrollToBottom(ImGuiTestRef ref);
    void        ScrollToItem(ImGuiTestRef ref, ImGuiAxis axis, ImGuiTestOpFlags flags = ImGuiTestOpFlags_None);
    void        ScrollToItemX(ImGuiTestRef ref);
    void        ScrollToItemY(ImGuiTestRef ref);
    void        ScrollToTabItem(ImGuiTabBar* tab_bar, ImGuiID tab_id);
    bool        ScrollErrorCheck(ImGuiAxis axis, float expected, float actual, int* remaining_attempts);
    void        ScrollVerifyScrollMax(ImGuiTestRef ref);

    // Low-level queries
    // Since 2022/06/25 to faciliate test code and reduce crashes: ItemInfo queries never return a NULL pointer, instead they return an empty instance (info->IsEmpty(), info->ID == 0).
    ImGuiTestItemInfo*  ItemInfo(ImGuiTestRef ref, ImGuiTestOpFlags flags = ImGuiTestOpFlags_None);
    ImGuiTestItemInfo*  ItemInfoOpenFullPath(ImGuiTestRef ref, ImGuiTestOpFlags flags = ImGuiTestOpFlags_None);
    ImGuiID             ItemInfoHandleWildcardSearch(const char* wildcard_prefix_start, const char* wildcard_prefix_end, const char* wildcard_suffix_start);
    ImGuiTestItemInfo*  ItemInfoNull();
    void                GatherItems(ImGuiTestItemList* out_list, ImGuiTestRef parent, int depth = -1);

    // Item/Widgets manipulation
    void        ItemAction(ImGuiTestAction action, ImGuiTestRef ref, ImGuiTestOpFlags flags = 0, void* action_arg = NULL);
    void        ItemClick(ImGuiTestRef ref, ImGuiMouseButton button = 0, ImGuiTestOpFlags flags = 0) { ItemAction(ImGuiTestAction_Click, ref, flags, (void*)(size_t)button); }
    void        ItemDoubleClick(ImGuiTestRef ref, ImGuiTestOpFlags flags = 0)           { ItemAction(ImGuiTestAction_DoubleClick, ref, flags); }
    void        ItemCheck(ImGuiTestRef ref, ImGuiTestOpFlags flags = 0)                 { ItemAction(ImGuiTestAction_Check, ref, flags); }
    void        ItemUncheck(ImGuiTestRef ref, ImGuiTestOpFlags flags = 0)               { ItemAction(ImGuiTestAction_Uncheck, ref, flags); }
    void        ItemOpen(ImGuiTestRef ref, ImGuiTestOpFlags flags = 0)                  { ItemAction(ImGuiTestAction_Open, ref, flags); }
    void        ItemClose(ImGuiTestRef ref, ImGuiTestOpFlags flags = 0)                 { ItemAction(ImGuiTestAction_Close, ref, flags); }
    void        ItemInput(ImGuiTestRef ref, ImGuiTestOpFlags flags = 0)                 { ItemAction(ImGuiTestAction_Input, ref, flags); }
    void        ItemNavActivate(ImGuiTestRef ref, ImGuiTestOpFlags flags = 0)           { ItemAction(ImGuiTestAction_NavActivate, ref, flags); }
    bool        ItemOpenFullPath(ImGuiTestRef);

    // Item/Widgets: Batch actions over an entire scope
    void        ItemActionAll(ImGuiTestAction action, ImGuiTestRef ref_parent, const ImGuiTestActionFilter* filter = NULL);
    void        ItemOpenAll(ImGuiTestRef ref_parent, int depth = -1, int passes = -1);
    void        ItemCloseAll(ImGuiTestRef ref_parent, int depth = -1, int passes = -1);

    // Item/Widgets: Helpers to easily set a value
    void        ItemInputValue(ImGuiTestRef ref, int v);
    void        ItemInputValue(ImGuiTestRef ref, float f);
    void        ItemInputValue(ImGuiTestRef ref, const char* str);

    // Item/Widgets: Drag and Mouse operations
    void        ItemHold(ImGuiTestRef ref, float time);
    void        ItemHoldForFrames(ImGuiTestRef ref, int frames);
    void        ItemDragOverAndHold(ImGuiTestRef ref_src, ImGuiTestRef ref_dst);
    void        ItemDragAndDrop(ImGuiTestRef ref_src, ImGuiTestRef ref_dst, ImGuiMouseButton button = 0);
    void        ItemDragWithDelta(ImGuiTestRef ref_src, ImVec2 pos_delta);

    // Helpers for Item/Widget state query
    void        ItemVerifyCheckedIfAlive(ImGuiTestRef ref, bool checked);

    // Helpers for Tab Bars widgets
    void        TabClose(ImGuiTestRef ref);
    bool        TabBarCompareOrder(ImGuiTabBar* tab_bar, const char** tab_order);

    // Helpers for Menus widgets
    void        MenuAction(ImGuiTestAction action, ImGuiTestRef ref);
    void        MenuActionAll(ImGuiTestAction action, ImGuiTestRef ref_parent);
    void        MenuClick(ImGuiTestRef ref)                 { MenuAction(ImGuiTestAction_Click, ref); }
    void        MenuCheck(ImGuiTestRef ref)                 { MenuAction(ImGuiTestAction_Check, ref); }
    void        MenuUncheck(ImGuiTestRef ref)               { MenuAction(ImGuiTestAction_Uncheck, ref); }
    void        MenuCheckAll(ImGuiTestRef ref_parent)       { MenuActionAll(ImGuiTestAction_Check, ref_parent); }
    void        MenuUncheckAll(ImGuiTestRef ref_parent)     { MenuActionAll(ImGuiTestAction_Uncheck, ref_parent); }

    // Helpers for Combo Boxes
    void        ComboClick(ImGuiTestRef ref);
    void        ComboClickAll(ImGuiTestRef ref);

    // Helpers for Tables
    void                        TableOpenContextMenu(ImGuiTestRef ref, int column_n = -1);
    ImGuiSortDirection          TableClickHeader(ImGuiTestRef ref, const char* label, ImGuiKeyChord key_mods = 0);
    void                        TableSetColumnEnabled(ImGuiTestRef ref, const char* label, bool enabled);
    void                        TableResizeColumn(ImGuiTestRef ref, int column_n, float width);
    const ImGuiTableSortSpecs*  TableGetSortSpecs(ImGuiTestRef ref);

    // Docking
#ifdef IMGUI_HAS_DOCK
    void        DockClear(const char* window_name, ...);
    void        DockInto(ImGuiTestRef src_id, ImGuiTestRef dst_id, ImGuiDir split_dir = ImGuiDir_None, bool is_outer_docking = false, ImGuiTestOpFlags flags = 0);
    void        UndockNode(ImGuiID dock_id);
    void        UndockWindow(const char* window_name);
    bool        WindowIsUndockedOrStandalone(ImGuiWindow* window);
    bool        DockIdIsUndockedOrStandalone(ImGuiID dock_id);
    void        DockNodeHideTabBar(ImGuiDockNode* node, bool hidden);
#endif

    // Performances
    void        PerfCalcRef();
    void        PerfCapture(const char* category = NULL, const char* test_name = NULL, const char* csv_file = NULL);

    // Obsolete functions
#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    // Obsoleted 2022/10/11
    ImGuiID     GetIDByInt(int n);                                      // Prefer using "$$123"
    ImGuiID     GetIDByInt(int n, ImGuiTestRef seed_ref);
    ImGuiID     GetIDByPtr(void* p);                                    // Prefer using "$$(ptr)0xFFFFFFFF"
    ImGuiID     GetIDByPtr(void* p, ImGuiTestRef seed_ref);
    // Obsoleted 2022/09/26
    void        KeyModDown(ImGuiModFlags mods)  { KeyDown(mods); }
    void        KeyModUp(ImGuiModFlags mods)    { KeyUp(mods); }
    void        KeyModPress(ImGuiModFlags mods) { KeyPress(mods); }
#endif

    // [Internal]
    // FIXME: Aim to remove this system...
    void        ForeignWindowsHideOverPos(ImVec2 pos, ImGuiWindow** ignore_list);
    void        ForeignWindowsUnhideAll();
};

//-------------------------------------------------------------------------
// [SECTION] Debugging macros
//-------------------------------------------------------------------------

// Debug: Temporarily suspend TestFunc to let user interactively inspect the GUI state (user will need to press the "Continue" button to resume TestFunc execution)
#define IM_SUSPEND_TESTFUNC()               do { if (ctx->SuspendTestFunc(__FILE__, __LINE__)) return; } while (0)

//-------------------------------------------------------------------------
// [SECTION] IM_CHECK macros
//-------------------------------------------------------------------------

// We embed every macro in a do {} while(0) statement as a trick to allow using them as regular single statement, e.g. if (XXX) IM_CHECK(A); else IM_CHECK(B)
// We leave the assert call (which will trigger a debugger break) outside of the check function to step out faster.
#define IM_CHECK_NO_RET(_EXPR)              do { if (ImGuiTestEngine_Check(__FILE__, __func__, __LINE__, ImGuiTestCheckFlags_None, (bool)(_EXPR), #_EXPR))          { IM_ASSERT(_EXPR); } } while (0)
#define IM_CHECK(_EXPR)                     do { if (ImGuiTestEngine_Check(__FILE__, __func__, __LINE__, ImGuiTestCheckFlags_None, (bool)(_EXPR), #_EXPR))          { IM_ASSERT(_EXPR); } if (!(bool)(_EXPR)) return; } while (0)
#define IM_CHECK_SILENT(_EXPR)              do { if (ImGuiTestEngine_Check(__FILE__, __func__, __LINE__, ImGuiTestCheckFlags_SilentSuccess, (bool)(_EXPR), #_EXPR)) { IM_ASSERT(0); } if (!(bool)(_EXPR)) return; } while (0)
#define IM_CHECK_RETV(_EXPR,_RETV)          do { if (ImGuiTestEngine_Check(__FILE__, __func__, __LINE__, ImGuiTestCheckFlags_None, (bool)(_EXPR), #_EXPR))          { IM_ASSERT(_EXPR); } if (!(bool)(_EXPR)) return _RETV; } while (0)
#define IM_CHECK_SILENT_RETV(_EXPR,_RETV)   do { if (ImGuiTestEngine_Check(__FILE__, __func__, __LINE__, ImGuiTestCheckFlags_SilentSuccess, (bool)(_EXPR), #_EXPR)) { IM_ASSERT(_EXPR); } if (!(bool)(_EXPR)) return _RETV; } while (0)
#define IM_ERRORF(_FMT,...)                 do { if (ImGuiTestEngine_Error(__FILE__, __func__, __LINE__, ImGuiTestCheckFlags_None, _FMT, __VA_ARGS__))              { IM_ASSERT(0); } } while (0)
#define IM_ERRORF_NOHDR(_FMT,...)           do { if (ImGuiTestEngine_Error(NULL, NULL, 0, ImGuiTestCheckFlags_None, _FMT, __VA_ARGS__))                             { IM_ASSERT(0); } } while (0)

template<typename T> void ImGuiTestEngineUtil_AppendStrValue(ImGuiTextBuffer& buf, T value)         { buf.appendf("???"); IM_UNUSED(value); } // FIXME-TESTS: Could improve with some template magic
template<> inline void ImGuiTestEngineUtil_AppendStrValue(ImGuiTextBuffer& buf, const char* value)  { buf.appendf("\"%s\"", value); }
template<> inline void ImGuiTestEngineUtil_AppendStrValue(ImGuiTextBuffer& buf, bool value)         { buf.append(value ? "true" : "false"); }
template<> inline void ImGuiTestEngineUtil_AppendStrValue(ImGuiTextBuffer& buf, ImS8 value)         { buf.appendf("%d", value); }
template<> inline void ImGuiTestEngineUtil_AppendStrValue(ImGuiTextBuffer& buf, ImU8 value)         { buf.appendf("%u", value); }
template<> inline void ImGuiTestEngineUtil_AppendStrValue(ImGuiTextBuffer& buf, ImS16 value)        { buf.appendf("%hd", value); }
template<> inline void ImGuiTestEngineUtil_AppendStrValue(ImGuiTextBuffer& buf, ImU16 value)        { buf.appendf("%hu", value); }
template<> inline void ImGuiTestEngineUtil_AppendStrValue(ImGuiTextBuffer& buf, ImS32 value)        { buf.appendf("%d", value); }
template<> inline void ImGuiTestEngineUtil_AppendStrValue(ImGuiTextBuffer& buf, ImU32 value)        { buf.appendf("0x%08X", value); } // Assuming ImGuiID
template<> inline void ImGuiTestEngineUtil_AppendStrValue(ImGuiTextBuffer& buf, ImS64 value)        { buf.appendf("%lld", value); }
template<> inline void ImGuiTestEngineUtil_AppendStrValue(ImGuiTextBuffer& buf, ImU64 value)        { buf.appendf("%llu", value); }
template<> inline void ImGuiTestEngineUtil_AppendStrValue(ImGuiTextBuffer& buf, float value)        { buf.appendf("%.3f", value); }
template<> inline void ImGuiTestEngineUtil_AppendStrValue(ImGuiTextBuffer& buf, double value)       { buf.appendf("%f", value); }
template<> inline void ImGuiTestEngineUtil_AppendStrValue(ImGuiTextBuffer& buf, ImVec2 value)       { buf.appendf("(%.3f, %.3f)", value.x, value.y); }
template<> inline void ImGuiTestEngineUtil_AppendStrValue(ImGuiTextBuffer& buf, const void* value)  { buf.appendf("%p", value); }
template<> inline void ImGuiTestEngineUtil_AppendStrValue(ImGuiTextBuffer& buf, ImGuiWindow* w)     { if (w) buf.appendf("\"%s\"", w->Name); else buf.append("NULL"); }

// Those macros allow us to print out the values of both lhs and rhs expressions involved in a check.
// FIXME: Could we move some more of that into a function?
#define IM_CHECK_OP(_LHS, _RHS, _OP, _RETURN)                       \
    do                                                              \
    {                                                               \
        auto __lhs = _LHS;  /* Cache to avoid side effects */       \
        auto __rhs = _RHS;                                          \
        bool __res = __lhs _OP __rhs;                               \
        ImGuiTextBuffer expr_buf;                                   \
        expr_buf.appendf("%s [", #_LHS);                            \
        ImGuiTestEngineUtil_AppendStrValue(expr_buf, __lhs);        \
        expr_buf.appendf("] " #_OP " %s [", #_RHS);                 \
        ImGuiTestEngineUtil_AppendStrValue(expr_buf, __rhs);        \
        expr_buf.append("]");                                       \
        if (ImGuiTestEngine_Check(__FILE__, __func__, __LINE__, ImGuiTestCheckFlags_None, __res, expr_buf.c_str())) \
            IM_ASSERT(__res);                                       \
        if (_RETURN && !__res)                                      \
            return;                                                 \
    } while (0)

#define IM_CHECK_STR_OP(_LHS, _RHS, _OP, _RETURN, _FLAGS)           \
    do                                                              \
    {                                                               \
        bool __res = ImGuiTestEngine_CheckStrOp(__FILE__, __func__, __LINE__, _FLAGS, #_OP, #_LHS, _LHS, #_RHS, _RHS); \
        if (!__res)                                                 \
            break;                                                  \
        IM_ASSERT(__res);                                           \
        if (_RETURN)                                                \
            return;                                                 \
    } while (0)

// Scalar compares
#define IM_CHECK_EQ(_LHS, _RHS)                     IM_CHECK_OP(_LHS, _RHS, ==, true)   // Equal
#define IM_CHECK_NE(_LHS, _RHS)                     IM_CHECK_OP(_LHS, _RHS, !=, true)   // Not Equal
#define IM_CHECK_LT(_LHS, _RHS)                     IM_CHECK_OP(_LHS, _RHS, < , true)   // Less Than
#define IM_CHECK_LE(_LHS, _RHS)                     IM_CHECK_OP(_LHS, _RHS, <=, true)   // Less or Equal
#define IM_CHECK_GT(_LHS, _RHS)                     IM_CHECK_OP(_LHS, _RHS, > , true)   // Greater Than
#define IM_CHECK_GE(_LHS, _RHS)                     IM_CHECK_OP(_LHS, _RHS, >=, true)   // Greater or Equal

// Scalar compares, without return on failure
#define IM_CHECK_EQ_NO_RET(_LHS, _RHS)              IM_CHECK_OP(_LHS, _RHS, ==, false)  // Equal
#define IM_CHECK_NE_NO_RET(_LHS, _RHS)              IM_CHECK_OP(_LHS, _RHS, !=, false)  // Not Equal
#define IM_CHECK_LT_NO_RET(_LHS, _RHS)              IM_CHECK_OP(_LHS, _RHS, < , false)  // Less Than
#define IM_CHECK_LE_NO_RET(_LHS, _RHS)              IM_CHECK_OP(_LHS, _RHS, <=, false)  // Less or Equal
#define IM_CHECK_GT_NO_RET(_LHS, _RHS)              IM_CHECK_OP(_LHS, _RHS, > , false)  // Greater Than
#define IM_CHECK_GE_NO_RET(_LHS, _RHS)              IM_CHECK_OP(_LHS, _RHS, >=, false)  // Greater or Equal

// String compares
#define IM_CHECK_STR_EQ(_LHS, _RHS)                 IM_CHECK_STR_OP(_LHS, _RHS, ==, true, ImGuiTestCheckFlags_None)
#define IM_CHECK_STR_NE(_LHS, _RHS)                 IM_CHECK_STR_OP(_LHS, _RHS, !=, true, ImGuiTestCheckFlags_None)
#define IM_CHECK_STR_EQ_NO_RET(_LHS, _RHS)          IM_CHECK_STR_OP(_LHS, _RHS, ==, false, ImGuiTestCheckFlags_None)
#define IM_CHECK_STR_NE_NO_RET(_LHS, _RHS)          IM_CHECK_STR_OP(_LHS, _RHS, !=, false, ImGuiTestCheckFlags_None)
#define IM_CHECK_STR_EQ_SILENT(_LHS, _RHS)          IM_CHECK_STR_OP(_LHS, _RHS, ==, true, ImGuiTestCheckFlags_SilentSuccess)

// Floating point compares
#define IM_CHECK_FLOAT_EQ_EPS(_LHS, _RHS)           IM_CHECK_LE(ImFabs(_LHS - (_RHS)), FLT_EPSILON)   // Float Equal
#define IM_CHECK_FLOAT_NEAR(_LHS, _RHS, _EPS)       IM_CHECK_LE(ImFabs(_LHS - (_RHS)), _EPS)
#define IM_CHECK_FLOAT_NEAR_NO_RET(_LHS, _RHS, _E)  IM_CHECK_LE_NO_RET(ImFabs(_LHS - (_RHS)), _E)

//-------------------------------------------------------------------------

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
