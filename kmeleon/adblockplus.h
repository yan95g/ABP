/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Adblock Plus.
 *
 * The Initial Developer of the Original Code is
 * Wladimir Palant.
 * Portions created by the Initial Developer are Copyright (C) 2006-2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * ***** END LICENSE BLOCK ***** */

#define KMELEON_PLUGIN_EXPORTS
#include "KMeleonConst.h"
#include "kmeleon_plugin.h"
#include "Utils.h"

#define MOZILLA_STRICT_API
#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIWindowWatcher.h"
#include "nsIObserver.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowInternal.h"
#include "nsPIDOMWindow.h"
#include "nsIXPConnect.h"
#include "nsIWebBrowser.h"
#include "nsIWebNavigation.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMEvent.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIURI.h"
#include "nsIJSContextStack.h"
#include "nsIScriptGlobalObject.h"
#include "nsIXPCScriptable.h"
#include "nsIChromeRegistrySea.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMEventListener.h"
#include "nsIPromptService.h"
#include "nsITimer.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIConsoleService.h"
#include "nsIScriptError.h"
#include "nsIScriptSecurityManager.h"
#include "nsIPrincipal.h"
#include "nsIWebBrowserChrome.h"
#include "nsIEmbeddingSiteWindow.h"
#include "nsIChromeEventHandler.h"
#include "imgIRequest.h"
#include "imgILoader.h"
#include "imgIDecoderObserver.h"
#include "gfxIImageFrame.h"
#include "nsIIOService.h"
#include "nsIComponentRegistrar.h"
#include "nsIProperties.h"
#include "nsDirectoryServiceDefs.h"
#include "nsILocalFile.h"
#include "nsIRDFService.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFResource.h"
#include "nsIRDFNode.h"
#include "nsXPCOM.h"
#include "nsEmbedString.h"
#include "jsapi.h"
#include "prmem.h"
#include "nsISupportsArray.h"

#define PLUGIN_NAME "Adblock Plus " ABP_VERSION
#define ADBLOCKPLUS_CONTRACTID "@mozilla.org/adblockplus;1"

enum {CMD_PREFERENCES, CMD_LISTALL, CMD_TOGGLEENABLED, CMD_IMAGE, CMD_OBJECT, CMD_LINK, CMD_FRAME, CMD_SEPARATOR, CMD_TOOLBAR, CMD_STATUSBAR, NUM_COMMANDS};
enum {LABEL_CONTEXT_IMAGE, LABEL_CONTEXT_OBJECT, LABEL_CONTEXT_LINK, LABEL_CONTEXT_FRAME, NUM_LABELS};

static char* context_labels[] = {
  "context.image...",
  "context.object...",
  "context.link...",
  "context.frame...",
};

static WORD context_commands[] = {
  CMD_IMAGE,
  CMD_OBJECT,
  CMD_LINK,
  CMD_FRAME
};

static char* images[] = {
  "chrome://adblockplus/skin/abp-enabled-16.png",
  "chrome://adblockplus/skin/abp-disabled-16.png",
  "chrome://adblockplus/skin/abp-whitelisted-16.png",
  "chrome://adblockplus/skin/abp-defunc-16.png",
};

extern HIMAGELIST hImages;

JS_STATIC_DLL_CALLBACK(void) Reporter(JSContext *cx, const char *message, JSErrorReport *rep);
JSBool JS_DLL_CALLBACK JSAddRootListener(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval);
JSBool JS_DLL_CALLBACK JSFocusWindow(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval);
JSBool JS_DLL_CALLBACK JSSetTopmostWindow(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval);
JSBool JS_DLL_CALLBACK JSShowToolbarContext(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval);
JSBool JS_DLL_CALLBACK JSOpenDialog(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval);
JSBool JS_DLL_CALLBACK JSSetIcon(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval);
JSBool JS_DLL_CALLBACK JSHideStatusBar(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval);
JSBool JS_DLL_CALLBACK FakeOpenTab(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval);
JSBool JS_DLL_CALLBACK JSResetContextMenu(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval);
JSBool JS_DLL_CALLBACK JSAddContextMenuItem(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval);
JSBool JS_DLL_CALLBACK JSCreateCommandID(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval);
JSBool JS_DLL_CALLBACK JSCreatePopupMenu(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval);
JSBool JS_DLL_CALLBACK JSAddMenuItem(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval);
JSBool JS_DLL_CALLBACK JSGetHWND(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval);
JSBool JS_DLL_CALLBACK JSSubclassDialogWindow(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval);
JSBool JS_DLL_CALLBACK JSGetWrapper(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK DialogWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK HookProc(int nCode, WPARAM wParam, LPARAM lParam);

static JSObject* UnwrapJSObject(nsISupports* native);
static nsISupports* UnwrapNative(JSContext* cx, JSObject* obj);
static void showContextMenu(HWND hWnd, PRBool status);

class abpJSContextHolder {
public:
  abpJSContextHolder() {
    mContext = nsnull;

    nsresult rv;
    mStack = do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv);
    if (NS_FAILED(rv))
      return;
  
    JSContext* cx;
    rv = mStack->GetSafeJSContext(&cx);
    if (NS_FAILED(rv))
      return;
  
    rv = mStack->Push(cx);
    if (NS_FAILED(rv))
      return;
  
    mContext = cx;
    mOldReporter = JS_SetErrorReporter(mContext, ::Reporter);
  }

  ~abpJSContextHolder() {
    if (mContext) {
      JS_SetErrorReporter(mContext, mOldReporter);

      nsresult rv;
      JSContext* cx;
      rv = mStack->Pop(&cx);
      NS_ASSERTION(NS_SUCCEEDED(rv) && cx == mContext, "JSContext push/pop mismatch");
    }
  }

  JSContext* get() {
    return mContext;
  }
private:
  nsCOMPtr<nsIThreadJSContextStack> mStack;
  JSContext* mContext;
  JSErrorReporter mOldReporter;
};

template<class T>
class abpList {
public:
  abpList() : buffer(nsnull), entries(0), bufSize(0) {}
  virtual ~abpList() {
    if (buffer != nsnull)
      PR_Free(buffer);
  }

protected:
  void addEntry(T& entry) {
    for (int i = 0; i < entries; i++) {
      if (!buffer[i].used) {
        buffer[i].used = PR_TRUE;
        buffer[i].data = entry;
        return;
      }
    }

    if (entries + 1 > bufSize) {
      bufSize += 8;
      if (buffer == nsnull)
        buffer = NS_STATIC_CAST(entryType*, PR_Malloc(bufSize * sizeof(entryType)));
      else
        buffer = NS_STATIC_CAST(entryType*, PR_Realloc(buffer, bufSize * sizeof(entryType)));
    }

    buffer[entries].used = PR_TRUE;
    buffer[entries].data = entry;
    entries++;
  }

  void removeEntry(int index) {
    buffer[index].used = PR_FALSE;
  }

  int getFirstIndex() {
    return getNextIndex(-1);
  }

  int getNextIndex(int index) {
    for (index++; index < entries; index++)
      if (buffer[index].used)
        return index;

    return -1;
  }

  T& getEntry(int index) {
    return buffer[index].data;
  }
private:
  typedef struct {
    PRBool used;
    T data;
  } entryType;

  int entries;
  int bufSize;
  entryType* buffer;
};

typedef struct {
  HWND hWnd;
  HWND hRebar;
  HWND hToolbar;
} ToolbarDataEntry;

class abpToolbarDataList : public abpList<ToolbarDataEntry> {
public:
  abpToolbarDataList() : currentIcon(3) {}
  void init(WORD command) {
    this->command = command;
  }

  void addToolbar(HWND hToolbar, HWND hRebar) {
    ToolbarDataEntry entry = {GetTopWindow(hRebar), hRebar, hToolbar};
    addEntry(entry);
  }

  void removeWindow(HWND hWnd) {
    for (int i = getFirstIndex(); i >= 0; i = getNextIndex(i))
      if (getEntry(i).hWnd == hWnd)
        removeEntry(i);
  }

  void invalidateToolbars() {
    for (int i = getFirstIndex(); i >= 0; i = getNextIndex(i))
      InvalidateRect(getEntry(i).hToolbar, NULL, TRUE);
  }

  void setToolbarIcon(int icon) {
    currentIcon = icon;

    for (int i = getFirstIndex(); i >= 0; i = getNextIndex(i))
      SendMessage(getEntry(i).hToolbar, TB_CHANGEBITMAP, command, MAKELPARAM(currentIcon, 0));
  }
private:
  int currentIcon;
  WORD command;
  REBARINFO info;
};

typedef void(*addStatusIconFunc)(HWND hWnd, int id, HICON hIcon, char* tpText);
typedef void(*removeStatusIconFunc)(HWND hWnd, int id);

class abpStatusBarList : public abpList<HWND> {
public:
  abpStatusBarList() : currentIcon(3), hidden(JS_FALSE) {}
  void init(HIMAGELIST hImages, WORD command, addStatusIconFunc addFunc, removeStatusIconFunc removeFunc) {
    this->hImages = hImages;
    this->command = command;
    this->addFunc = addFunc;
    this->removeFunc = removeFunc;
  }
  
  void addStatusBar(HWND hWnd) {
    addEntry(hWnd);
    if (!hidden)
      addFunc(hWnd, command, ImageList_GetIcon(hImages, currentIcon, ILD_TRANSPARENT), NULL);
  }

  void removeStatusBar(HWND hWnd) {
    for (int i = getFirstIndex(); i >= 0; i = getNextIndex(i))
      if (getEntry(i) == hWnd)
        removeEntry(i);

    if (!hidden)
      removeFunc(hWnd, command);
  }

  void invalidateStatusBars() {
    setStatusIcon(-1);
  }

  void setStatusIcon(int icon) {
    if (icon == currentIcon)
      return;

    if (icon >= 0)
      currentIcon = icon;

    if (hidden)
      return;

    for (int i = getFirstIndex(); i >= 0; i = getNextIndex(i)) {
      HWND hWnd = getEntry(i);
      removeFunc(hWnd, command);
      addFunc(hWnd, command, ImageList_GetIcon(hImages, currentIcon, ILD_TRANSPARENT), NULL);
    }
  }

  void setHidden(JSBool hide) {
    if (hide == hidden)
      return;

    hidden = hide;
    if (hidden) {
      for (int i = getFirstIndex(); i >= 0; i = getNextIndex(i))
        removeFunc(getEntry(i), command);
    }
    else {
      for (int i = getFirstIndex(); i >= 0; i = getNextIndex(i))
        addFunc(getEntry(i), command, ImageList_GetIcon(hImages, currentIcon, ILD_TRANSPARENT), NULL);
    }
  }
private:
  HIMAGELIST hImages;
  WORD command;
  addStatusIconFunc addFunc;
  removeStatusIconFunc removeFunc;
  int currentIcon;
  JSBool hidden;
};

class abpWrapper : public nsIDOMEventListener,
                   public nsIClassInfo,
                   public nsIXPCScriptable,
                   imgIDecoderObserver {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER
  NS_DECL_NSICLASSINFO
  NS_DECL_NSIXPCSCRIPTABLE
  NS_DECL_IMGIDECODEROBSERVER
  NS_DECL_IMGICONTAINEROBSERVER

  abpWrapper() {
    hImages = ImageList_Create(16, 16, ILC_COLOR32, sizeof(images)/sizeof(images[0]), 0);
  };
  virtual ~abpWrapper() {
    ImageList_Destroy(hImages);
  };

  static LONG DoMessage(LPCSTR to, LPCSTR from, LPCSTR subject, LONG data1, LONG data2);
  static PRBool Load();
  static void Setup();
  static void Create(HWND parent);
  static void Close(HWND parent);
  static void Config(HWND parent);
  static void Quit();
  static void DoMenu(HMENU menu, LPSTR action, LPSTR string);
  static INT DoAccel(LPSTR action);
  static void DoRebar(HWND hRebar);
  virtual JSObject* OpenDialog(char* url, char* target, char* features, nsISupportsArray* args);
  virtual nsresult OpenTab(const char* url);
  virtual void SetCurrentIcon(int icon) {toolbarList.setToolbarIcon(icon);statusbarList.setStatusIcon(icon);}
  virtual void HideStatusBar(JSBool hide) {statusbarList.setHidden(hide);}
  virtual JSObject* GetGlobalObject(nsIDOMWindow* wnd);
  virtual void AddContextMenuItem(WORD command, char* label);
  virtual void ResetContextMenu();
  virtual UINT CreateCommandID() {return kFuncs->GetCommandIDs(1);}
  static WNDPROC SubclassWindow(HWND hWnd, WNDPROC newWndProc);
protected:
  static kmeleonFunctions* kFuncs;
  static nsCOMPtr<nsIWindowWatcher> watcher;
  static nsCOMPtr<nsIIOService> ioService;
  static nsCOMPtr<nsIPrincipal> systemPrincipal;
  static abpToolbarDataList toolbarList;
  static abpStatusBarList statusbarList;

  nsCOMPtr<imgIRequest> imageRequest;
  int currentImage;

  static PRBool PatchComponent(JSContext* cx);
  static PRBool CreateFakeBrowserWindow(JSContext* cx, JSObject* parent);
  static INT CommandByName(LPSTR action);
  static void ReadAccelerator(nsIPrefBranch* branch, const char* pref, const char* command);
  virtual void LoadImage(int index);
};

kmeleonPlugin kPlugin = {
  KMEL_PLUGIN_VER,
  PLUGIN_NAME,
  &abpWrapper::DoMessage
};

extern "C" {
  KMELEON_PLUGIN kmeleonPlugin *GetKmeleonPlugin() {
    return &kPlugin;
  }
}
