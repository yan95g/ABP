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

/************************
 * JavaScript callbacks *
 ************************/
 
JSBool JS_DLL_CALLBACK JSSetIcon(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval) {
  *rval = JSVAL_VOID;

  if (argc == 1)
    wrapper->SetCurrentIcon(JSVAL_TO_INT(argv[0]));

  return JS_TRUE;
}

JSBool JS_DLL_CALLBACK JSHideStatusBar(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval) {
  *rval = JSVAL_VOID;

  if (argc == 1)
    wrapper->HideStatusBar(JSVAL_TO_BOOLEAN(argv[0]));

  return JS_TRUE;
}

JSBool JS_DLL_CALLBACK JSOpenTab(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval) {
  *rval = JSVAL_VOID;

  char* url;
  int32 wnd;
  if (!JS_ConvertArguments(cx, argc, argv, "sj", &url, &wnd))
    return JS_FALSE;

  OpenTab(url, (HWND)wnd);
  return JS_TRUE;
}

TCHAR* menus[] = {_T("DocumentPopup"), _T("DocumentImagePopup"), _T("TextPopup"),
                  _T("LinkPopup"), _T("ImageLinkPopup"), _T("ImagePopup"),
                  _T("FrameDocumentPopup"), _T("FrameDocumentImagePopup"), _T("FrameTextPopup"),
                  _T("FrameLinkPopup"), _T("FrameImageLinkPopup"), _T("FrameImagePopup"),
                  NULL};

JSBool JS_DLL_CALLBACK JSResetContextMenu(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval) {
  *rval = JSVAL_VOID;

  for (int i = 0; menus[i]; i++) {
    HMENU hMenu = kFuncs->GetMenu(menus[i]);
    if (hMenu) {
      int count = GetMenuItemCount(hMenu);
      for (int j = 0; j < count; j++) {
        WORD id = GetMenuItemID(hMenu, j) - cmdBase;
        if (id < NUM_COMMANDS)
          RemoveMenu(hMenu, j--, MF_BYPOSITION);
      }
    }
  }

  return JS_TRUE;
}

JSBool JS_DLL_CALLBACK JSAddContextMenuItem(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval) {
  *rval = JSVAL_VOID;

  int32 item;
  if (!JS_ConvertArguments(cx, argc, argv, "j", &item))
    return JS_FALSE;

  if (item < 0 || item >= NUM_LABELS)
    return JS_TRUE;

  MENUITEMINFO info = {0};
  info.cbSize = sizeof info;
  info.fMask = MIIM_TYPE;

  UINT drawFlag;
  for (int i = 0; menus[i]; i++) {
    HMENU hMenu = kFuncs->GetMenu(menus[i]);
    if (hMenu) {
      drawFlag = MF_OWNERDRAW;

      int count = GetMenuItemCount(hMenu);
      if (count > 0) {
        WORD id = GetMenuItemID(hMenu, count - 1) - cmdBase;
        if (id >= NUM_COMMANDS)
          AppendMenuA(hMenu, MF_SEPARATOR, cmdBase + CMD_SEPARATOR, NULL);

        // Only use MF_OWNERDRAW flag if other menu items have it as well
        if (GetMenuItemInfo(hMenu, 0, TRUE, &info) && !(info.fType & MFT_OWNERDRAW))
          drawFlag = MF_STRING;
      }
      AppendMenuA(hMenu, drawFlag, cmdBase + context_commands[item], labelValues[item]);
    }
  }
  return JS_TRUE;
}

JSBool JS_DLL_CALLBACK JSCreateCommandID(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval) {
  *rval = INT_TO_JSVAL(wrapper->CreateCommandID());

  return JS_TRUE;
}

JSBool JS_DLL_CALLBACK JSCreatePopupMenu(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval) {
  HMENU ret = CreatePopupMenu();
  *rval = INT_TO_JSVAL(ret);

  return JS_TRUE;
}

JSBool JS_DLL_CALLBACK JSAddMenuItem(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval) {
  *rval = JSVAL_VOID;

  int32 menu;
  int32 type;
  int32 menuID;
  char* label;
  JSBool default;
  JSBool disabled;
  JSBool checked;
  if (!JS_ConvertArguments(cx, argc, argv, "jjjsbbb", &menu, &type, &menuID, &label, &default, &disabled, &checked))
    return JS_FALSE;
  
  HMENU hMenu = (HMENU)menu;

  MENUITEMINFO info = {0};
  info.cbSize = sizeof info;
  info.fMask = MIIM_STATE | MIIM_SUBMENU | MIIM_TYPE;
  if (menuID >= 0 && !disabled)
    info.fMask |= MIIM_ID;
  info.fType = (type < 0 ? MFT_SEPARATOR : MFT_STRING);
  info.fState = (disabled ? MFS_GRAYED : MFS_ENABLED);
  if (checked)
    info.fState |= MFS_CHECKED;
  if (default)
    info.fState |= MFS_DEFAULT;
  info.wID = (UINT)menuID;
  info.hSubMenu = type > 0 ? (HMENU)type : NULL;
  info.dwTypeData = label;

  InsertMenuItem(hMenu, -1, TRUE, &info);

  return JS_TRUE;
}

JSBool JS_DLL_CALLBACK JSGetHWND(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval) {
  *rval = JSVAL_NULL;

  if (argc != 1) {
    JS_ReportError(cx, "getHWND: wrong number of arguments");
    return JS_FALSE;
  }

  nsCOMPtr<nsIEmbeddingSiteWindow> wnd  = do_QueryInterface(UnwrapNative(cx, JSVAL_TO_OBJECT(argv[0])));
  if (wnd == nsnull)
    return JS_TRUE;

  void* hWnd;
  nsresult rv = wnd->GetSiteWindow(&hWnd);
  if (NS_FAILED(rv))
    return JS_TRUE;

  *rval = INT_TO_JSVAL((int32)hWnd);
  return JS_TRUE;
}

JSBool JS_DLL_CALLBACK JSSubclassDialogWindow(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval) {
  *rval = JSVAL_VOID;

  int32 wnd;
  if (!JS_ConvertArguments(cx, argc, argv, "j", &wnd))
    return JS_FALSE;
  
  origDialogWndProc = SubclassWindow((HWND)wnd, &DialogWndProc);

  return JS_TRUE;
}

JSBool JS_DLL_CALLBACK JSAddRootListener(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval) {
  *rval = JSVAL_VOID;

  JSObject* wndObject;
  char* event;
  JSBool capture;
  if (!JS_ConvertArguments(cx, argc, argv, "osb", &wndObject, &event, &capture))
    return JS_FALSE;

  nsCOMPtr<nsPIDOMWindow> privateWnd = do_QueryInterface(UnwrapNative(cx, wndObject));
  if (privateWnd == nsnull)
    return JS_TRUE;

  nsCOMPtr<nsPIDOMWindow> rootWnd = privateWnd->GetPrivateRoot();
  if (rootWnd == nsnull)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIChromeEventHandler> chromeHandler = rootWnd->GetChromeEventHandler();
  if (chromeHandler == nsnull)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(chromeHandler);
  if (target == nsnull)
    return NS_ERROR_FAILURE;

  target->AddEventListener(NS_ConvertASCIItoUTF16(event), wrapper, capture);
  return JS_TRUE;
}

JSBool JS_DLL_CALLBACK JSFocusWindow(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval) {
  *rval = JSVAL_VOID;

  int32 wnd;
  if (!JS_ConvertArguments(cx, argc, argv, "j", &wnd))
    return JS_FALSE;

  BringWindowToTop((HWND)wnd);
  return JS_TRUE;
}

JSBool JS_DLL_CALLBACK JSSetTopmostWindow(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval) {
  *rval = JSVAL_VOID;

  int32 wnd;
  if (!JS_ConvertArguments(cx, argc, argv, "j", &wnd))
    return JS_FALSE;

  SetWindowPos((HWND)wnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOSIZE);
  return JS_TRUE;
}

JSBool JS_DLL_CALLBACK JSShowToolbarContext(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval) {
  *rval = JSVAL_VOID;

  int32 wnd;
  if (!JS_ConvertArguments(cx, argc, argv, "j", &wnd))
    return JS_FALSE;

  ShowContextMenu((HWND)wnd, PR_FALSE);

  return JS_TRUE;
}

JSBool JS_DLL_CALLBACK JSGetScriptable(JSContext *cx, JSObject *obj, jsval id, jsval *vp) {
  nsresult rv;

  nsCOMPtr<nsIXPConnect> xpc = do_GetService(nsIXPConnect::GetCID());
  if (xpc == nsnull)
    return JS_FALSE;

  nsCOMPtr<nsIXPConnectJSObjectHolder> wrapperHolder;
  rv = xpc->WrapNative(cx, JS_GetParent(cx, obj), NS_STATIC_CAST(nsIXPCScriptable*, wrapper), NS_GET_IID(nsISupports), getter_AddRefs(wrapperHolder));
  if (NS_FAILED(rv))
    return JS_FALSE;

  JSObject* result;
  rv = wrapperHolder->GetJSObject(&result);
  if (NS_FAILED(rv))
    return JS_FALSE;

  *vp = OBJECT_TO_JSVAL(result);
  return JS_TRUE;
}

/*********************************************
 * Custom window method/property definitions *
 *********************************************/

JSFunctionSpec window_methods[] = {
  {"setIcon", JSSetIcon, 1, 0, 0},
  {"hideStatusBar", JSHideStatusBar, 1, 0, 0},
  {"openTab", JSOpenTab, 1, 0, 0},
  {"resetContextMenu", JSResetContextMenu, 0, 0, 0},
  {"addContextMenuItem", JSAddContextMenuItem, 1, 0, 0},
  {"createCommandID", JSCreateCommandID, 0, 0, 0},
  {"createPopupMenu", JSCreatePopupMenu, 0, 0, 0},
  {"addMenuItem", JSAddMenuItem, 7, 0, 0},
  {"getHWND", JSGetHWND, 1, 0, 0},
  {"subclassDialogWindow", JSSubclassDialogWindow, 1, 0, 0},
  {"addRootListener", JSAddRootListener, 3, 0, 0},
  {"focusWindow", JSFocusWindow, 1, 0, 0},
  {"setTopmostWindow", JSSetTopmostWindow, 1, 0, 0},
  {"showToolbarContext", JSShowToolbarContext, 1, 0, 0},
  {NULL},
};
JSPropertySpec window_properties[] = {
  {"scriptable", 2, JSPROP_READONLY|JSPROP_PERMANENT, JSGetScriptable, nsnull},
  {NULL},
};