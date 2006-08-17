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
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * Fake protocol handler for the abp: protocol.
 * This file is included from nsAdblockPlus.js.
 */

var protocol = {
  defaultPort: 0,
  protocolFlags: Components.interfaces.nsIProtocolHandler.URI_NORELATIVE || Components.interfaces.nsIProtocolHandler.ALLOWS_PROXY,
  scheme: "abp",
  allowPort: function() {return false},

  newURI: function(spec, originCharset, baseURI) {
    var url = Components.classes["@mozilla.org/network/standard-url;1"]
                        .createInstance(Components.interfaces.nsIStandardURL);
    url.init(Components.interfaces.nsIStandardURL.URLTYPE_STANDARD,
              0, spec, originCharset, baseURI);

    return url.QueryInterface(Components.interfaces.nsIURI);
  },

  newChannel: function(uri) {
    return new ABPChannel(uri);
  }
};

function ABPChannel(uri) {
  this.URI = this.originalURI = uri;
}

ABPChannel.prototype = {
  contentLength: 0,
  owner: null,
  securityInfo: null,
  notificationCallbacks: null,
  loadFlags: 0,
  loadGroup: null,
  name: null,
  status: Components.results.NS_OK,

  addSubscription: function(data) {
    for (var i = 0; i < prefs.subscriptions.length; i++)
      if (prefs.subscriptions[i].url == data.url)
        return;

    var subscription = prefs.subscriptionFromURL(data.url);
    if (!subscription)
      return;

    subscription.title = data.title;
    subscription.disabled = data.disabled;
    subscription.autoDownload = data.autoDownload;
    prefs.subscriptions.push(subscription);

    synchronizer.notifyListeners(subscription, "add");
    synchronizer.execute(subscription);
  },

  open: function() {
    if (/^abp:\/*subscribe\/*\?(.*)/i.test(this.URI.spec)) {
      var unescape = Components.classes["@mozilla.org/intl/texttosuburi;1"]
                               .getService(Components.interfaces.nsITextToSubURI);

      var params = RegExp.$1.split('&');
      var title = null;
      var url = null;
      for (var i = 0; i < params.length; i++) {
        var parts = params[i].split('=', 2);
        if (parts.length == 2 && parts[0] == 'title')
          title = decodeURIComponent(unescape.unEscapeNonAsciiURI(this.URI.originCharset, parts[1]));
        if (parts.length == 2 && parts[0] == 'location')
          url = decodeURIComponent(unescape.unEscapeNonAsciiURI(this.URI.originCharset, parts[1]));
      }

      if (url && /\S/.test(url)) {
        if (!title || !/\S/.test(title))
          title = url;

        var result = {};
        var subscription = {url: url, title: title, disabled: false, external: false, autoDownload: true};

        var browser = windowMediator.getMostRecentWindow("navigator:browser");
        if (browser)
          browser.openDialog("chrome://adblockplus/content/subscription.xul", "_blank",
                             "chrome,centerscreen,modal", abp, prefs, subscription, result);

          if ("url" in result)
            this.addSubscription(result);
      }
    }
    return Components.results.NS_ERROR_NO_CONTENT;
  },

  asyncOpen: function() {
    return this.open();
  },  
  isPending: function() {
    return false
  },
  cancel: function(status) {
    this.status = status;
  },
  suspend: function() {
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
  },
  resume: function() {
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
  },

  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsIChannel) ||
        iid.equals(Components.interfaces.nsIRequest) ||
        iid.equals(Components.interfaces.nsISupports))
      return this; 

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
};
