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
 * Manages synchronization of filter subscriptions.
 * This file is included from nsAdblockPlus.js.
 */

var synchronizer = {
  executing: new HashTable(),
  listeners: [],
  timer: null,

  init: function() {
    this.timer = createTimer(this.synchronizeCallback, 10000/*300000*/);
    this.timer.type = this.timer.TYPE_REPEATING_SLACK;
  },

  synchronizeCallback: function() {
    synchronizer.timer.delay = 36000/*3600000*/;

    for (var i = 0; i < prefs.subscriptions.length; i++) {
      var subscription = prefs.subscriptions[i];
      if (subscription.special || !subscription.autoDownload || subscription.external)
        continue;
  
      // Get the number of hours since last download
      var interval = (new Date().getTime()/1000 - subscription.lastSuccess) / 3600;
      /*if (interval > prefs.synchronizationinterval)*/
        synchronizer.execute(subscription);
    }
  },

  // Adds a new handler to be notified whenever synchronization status changes
  addListener: function(handler) {
    this.listeners.push(handler);
  },
  
  // Removes a handler
  removeListener: function(handler) {
    for (var i = 0; i < this.listeners.length; i++)
      if (this.listeners[i] == handler)
        this.listeners.splice(i--, 1);
  },

  // Calls all listeners
  notifyListeners: function(subscription, status) {
    for (var i = 0; i < this.listeners.length; i++)
      this.listeners[i](subscription, status);
  },

  isExecuting: function(url) {
    return this.executing.has(url);
  },

  readPatterns: function(subscription, text) {
    var lines = text.split(/[\r\n]+/);
    for (var i = 0; i < lines.length; i++) {
      lines[i] = lines[i].replace(/\s/g, "");
      if (!lines[i])
        lines.splice(i--, 1);
    }
    if (!/\[Adblock\]/i.test(lines[0])) {
      this.setError(subscription, "synchronize_invalid_data");
      return;
    }

    subscription.lastDownload = subscription.lastSuccess = new Date().getTime() / 1000;
    subscription.downloadStatus = "synchronize_ok";
    subscription.patterns = [];
    for (var i = 1; i < lines.length; i++) {
      var pattern = prefs.patternFromText(lines[i]);
      if (pattern)
        subscription.patterns.push(pattern);
    }
    prefs.savePatterns();
    this.notifyListeners(subscription, "ok");
  },

  setError: function(subscription, error) {
    this.executing.remove(subscription.url);
    subscription.lastDownload = new Date().getTime() / 1000;
    subscription.downloadStatus = error;
    prefs.savePatterns();
    this.notifyListeners(subscription, "error");
  },

  execute: function(subscription) {
    var url = subscription.url;
    if (this.executing.has(url))
      return;

    try {
      var request = Components.classes["@mozilla.org/xmlextras/xmlhttprequest;1"]
                              .createInstance(Components.interfaces.nsIJSXMLHttpRequest);
      request.open("GET", url);
      request.channel.loadFlags = request.channel.loadFlags |
                                  request.channel.INHIBIT_CACHING |
                                  request.channel.LOAD_BYPASS_CACHE;
    }
    catch (e) {
      this.setError(subscription, "synchronize_invalid_url");
      return;
    }

    request.onerror = function(ev) {
      if (!prefs.knownSubscriptions.has(url))
        return;

      synchronizer.setError(prefs.knownSubscriptions.get(url), "synchronize_connection_error");
    };

    request.onload = function(ev) {
      synchronizer.executing.remove(url);
      if (prefs.knownSubscriptions.has(url))
        synchronizer.readPatterns(prefs.knownSubscriptions.get(url), ev.target.responseText);
    };

    this.executing.put(url, request);
    this.notifyListeners(subscription, "executing");

    try {
      request.send(null);
    }
    catch (e) {
      this.setError(subscription, "synchronize_connection_error");
    }

    // prevent cyclic references through closures
    request = null;
  }
};

synchronizer.init();
abp.synchronizer = synchronizer;
