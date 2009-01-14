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
 * Portions created by the Initial Developer are Copyright (C) 2006-2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * FilterStorage class responsible to managing user's subscriptions and filters.
 * This file is included from nsAdblockPlus.js.
 */

var dirService = Components.classes["@mozilla.org/file/directory_service;1"]
                           .getService(Components.interfaces.nsIProperties);

/**
 * This class reads user's filters from disk, manages them in memory and writes them back.
 * @class
 */
var filterStorage =
{
  /**
   * Version number of the filter storage file format.
   * @type Integer
   */
  formatVersion: 3,

  /**
   * Map of properties listed in the filter storage file before the sections
   * start. Right now this should be only the format version.
   */
  fileProperties: {},

  /**
   * List of filter subscriptions containing all filters
   * @type Array of Subscription
   */
  subscriptions: [],

  /**
   * Map of subscriptions already on the list, by their URL/identifier
   * @type Object
   */
  knownSubscriptions: {__proto__: null},

  /**
   * File that the filter list has been loaded from and should be saved to
   * @type nsIFile
   */
  file: null,

  /**
   * List of observers for subscription changes (addition, deletion)
   * @type Array of function(String, Array of Subscription)
   */
  subscriptionObservers: [],

  /**
   * List of observers for filter changes (addition, deletion)
   * @type Array of function(String, Array of Filter)
   */
  filterObservers: [],

  /**
   * Adds an observer for subscription changes (addition, deletion)
   * @param {function(String, Array of Subscription)} observer
   */
  addSubscriptionObserver: function(observer)
  {
    if (this.subscriptionObservers.indexOf(observer) >= 0)
      return;

    this.subscriptionObservers.push(observer);
  },

  /**
   * Removes a subscription observer previosly added with addSubscriptionObserver
   * @param {function(String, Array of Subscription)} observer
   */
  removeSubscriptionObserver: function(observer)
  {
    let index = this.subscriptionObservers.indexOf(observer);
    if (index >= 0)
      this.subscriptionObservers.splice(index, 1);
  },

  /**
   * Calls subscription observers after a change
   * @param {String} action change code ("add", "remove", "enable", "disable", "update", "updateinfo", "reload")
   * @param {Array of Subscription} subscriptions subscriptions the change applies to
   */
  triggerSubscriptionObservers: function(action, subscriptions)
  {
    for each (let observer in this.subscriptionObservers)
      observer(action, subscriptions);
  },

  /**
   * Adds an observer for filter changes (addition, deletion)
   * @param {function(String, Array of Filter)} observer
   */
  addFilterObserver: function(observer)
  {
    if (this.filterObservers.indexOf(observer) >= 0)
      return;

    this.filterObservers.push(observer);
  },

  /**
   * Removes a filter observer previosly added with addFilterObserver
   * @param {function(String, Array of Filter)} observer
   */
  removeFilterObserver: function(observer)
  {
    let index = this.filterObservers.indexOf(observer);
    if (index >= 0)
      this.filterObservers.splice(index, 1);
  },

  /**
   * Calls filter observers after a change
   * @param {String} action change code ("add", "remove", "enable", "disable", "hit")
   * @param {Array of Filter} filters the change applies to
   */
  triggerFilterObservers: function(action, filters)
  {
    for each (let observer in this.filterObservers)
      observer(action, filters);
  },

  /**
   * Joins subscription's filters to the subscription without any notifications.
   * @param {Subscription} subscription filter subscription that should be connected to its filters
   */
  _addSubscriptionFilters: function(subscription)
  {
    if (!(subscription.url in this.knownSubscriptions))
      return;

    for each (let filter in subscription.filters)
      filter.subscriptions.push(subscription);
  },

  /**
   * Adds a filter subscription to the list
   * @param {Subscription} subscription filter subscription to be added
   * @param {Boolean} silent  if true, no observers will be triggered (to be used when filter list is reloaded)
   */
  addSubscription: function(subscription, silent)
  {
    if (subscription.url in this.knownSubscriptions)
      return;

    this.subscriptions.push(subscription);
    this.knownSubscriptions[subscription.url] = subscription;
    this._addSubscriptionFilters(subscription);

    if (!silent)
      this.triggerSubscriptionObservers("add", [subscription]);
  },

  /**
   * Removes subscription's filters from the subscription without any notifications.
   * @param {Subscription} subscription filter subscription to be removed
   */
  _removeSubscriptionFilters: function(subscription)
  {
    if (!(subscription.url in this.knownSubscriptions))
      return;

    for each (let filter in subscription.filters)
    {
      let i = filter.subscriptions.indexOf(subscription);
      if (i >= 0)
        filter.subscriptions.splice(i, 1);
    }
  },

  /**
   * Removes a filter subscription from the list
   * @param {Subscription} subscription filter subscription to be removed
   * @param {Boolean} silent  if true, no observers will be triggered (to be used when filter list is reloaded)
   */
  removeSubscription: function(subscription, silent)
  {
    for (let i = 0; i < this.subscriptions.length; i++)
    {
      if (this.subscriptions[i].url == subscription.url)
      {
        this._removeSubscriptionFilters(subscription);

        this.subscriptions.splice(i--, 1);
        delete this.knownSubscriptions[subscription.url];
        if (!silent)
          this.triggerSubscriptionObservers("remove", [subscription]);
        return;
      }
    }
  },

  /**
   * Replaces the list of filters in a subscription by a new list
   * @param {Subscription} subscription filter subscription to be updated
   * @param {Array of Filter} filters new filter lsit
   */
  updateSubscriptionFilters: function(subscription, filters)
  {
    this._removeSubscriptionFilters(subscription);
    subscription.oldFilters = subscription.filters;
    subscription.filters = filters;
    this._addSubscriptionFilters(subscription);
    this.triggerSubscriptionObservers("update", [subscription]);
    delete subscription.oldFilters;
  },

  /**
   * Adds a user-defined filter to the list
   * @param {Filter} filter
   * @param {Boolean} silent  if true, no observers will be triggered (to be used when filter list is reloaded)
   */
  addFilter: function(filter, silent)
  {
    let subscription = null;
    for each (let s in this.subscriptions)
    {
      if (s instanceof SpecialSubscription && s.isFilterAllowed(filter))
      {
        if (s.filters.indexOf(filter) >= 0)
          return;

        if (!subscription || s.priority > subscription.priority)
          subscription = s;
      }
    }

    if (!subscription)
      return;

    filter.subscriptions.push(subscription);
    subscription.filters.push(filter);
    if (!silent)
      this.triggerFilterObservers("add", [filter]);
  },

  /**
   * Removes a user-defined filter from the list
   * @param {Filter} filter
   * @param {Boolean} silent  if true, no observers will be triggered (to be used when filter list is reloaded)
   */
  removeFilter: function(filter, silent)
  {
    for (let i = 0; i < filter.subscriptions.length; i++)
    {
      let subscription = filter.subscriptions[i];
      if (subscription instanceof SpecialSubscription)
      {
        for (let j = 0; j < subscription.filters.length; j++)
        {
          if (subscription.filters[j].text == filter.text)
          {
            filter.subscriptions.splice(i, 1);
            subscription.filters.splice(j, 1);
            if (!silent)
              this.triggerFilterObservers("remove", [filter]);
            return;
          }
        }
      }
    }
  },

  /**
   * Increases the hit count for a filter by one
   * @param {Filter} filter
   */
  increaseHitCount: function(filter)
  {
    if (!prefs.savestats || prefs.privateBrowsing || !(filter instanceof ActiveFilter))
      return;

    filter.hitCount++;
    filter.lastHit = Date.now();
    this.triggerFilterObservers("hit", [filter]);
  },

  /**
   * Resets hit count for some filters
   * @param {Array of Filter} filters  filters to be reset, if null all filters will be reset
   */
  resetHitCounts: function(filters)
  {
    if (!filters)
    {
      filters = [];
      for each (let filter in Filter.knownFilters)
        filters.push(filter);
    }
    for each (let filter in filters)
    {
      filter.hitCount = 0;
      filter.lastHit = 0;
    }
    this.triggerFilterObservers("hit", filters);
  },

  /**
   * Loads all subscriptions from the disk
   */
  loadFromDisk: function()
  {
    timeLine.log("up to loadFromDisk()");

    this.subscriptions = [];
    this.knownSubscriptions = {__proto__: null};

    function getFileByPath(path)
    {
      try {
        // Assume an absolute path first
        let file = Components.classes["@mozilla.org/file/local;1"]
                             .createInstance(Components.interfaces.nsILocalFile);
        file.initWithPath(path);
        return file;
      } catch (e) {}

      try {
        // Try relative path now
        let profileDir = dirService.get("ProfD", Components.interfaces.nsIFile);
        let file = Components.classes["@mozilla.org/file/local;1"]
                         .createInstance(Components.interfaces.nsILocalFile);
        file.setRelativeDescriptor(profileDir, path);
        return file;
      } catch (e) {}

      return null;
    }

    this.file = getFileByPath(prefs.patternsfile);
    if (!this.file && "patternsfile" in prefs.prefList)
      this.file = getFileByPath(this.prefList.patternsfile[2]);   // Try default

    if (!this.file)
      dump("Adblock Plus: Failed to resolve filter file location from extensions.adblockplus.patternsfile preference\n");

    let stream = null;
    if (this.file)
    {
      try {
        var fileStream = Components.classes["@mozilla.org/network/file-input-stream;1"]
                           .createInstance(Components.interfaces.nsIFileInputStream);
        fileStream.init(this.file, 0x01, 0444, 0);

        stream = Components.classes["@mozilla.org/intl/converter-input-stream;1"]
                           .createInstance(Components.interfaces.nsIConverterInputStream);
        stream.init(fileStream, "UTF-8", 16384, Components.interfaces.nsIConverterInputStream.DEFAULT_REPLACEMENT_CHARACTER);
        stream = stream.QueryInterface(Components.interfaces.nsIUnicharLineInputStream);
      }
      catch (e) {
        dump("Adblock Plus: Failed to read filters from file " + this.file.path + ": " + e + "\n");
        stream = null;
      }
    }

    let userFilters = null;
    if (stream)
    {
      userFilters = this.parseIniFile(stream);

      stream.close();
    }
    else
    {
      // Probably the first time we run - try to import settings from Adblock
      let importBranch = prefService.getBranch("adblock.");

      try {
        if (importBranch.prefHasUserValue("patterns"))
          for each (let text in importBranch.getCharPref("patterns").split(" "))
            this.addFilter(Filter.fromText(text), true);
      } catch (e) {}

      try {
        for each (let url in importBranch.getCharPref("syncpath").split("|"))
          if (!(url in this.knownSubscriptions))
            this.addSubscription(Subscription.fromURL(url));
      } catch (e) {}
    }

    // Add missing special subscriptions if necessary
    for each (let specialSubscription in ["~il~", "~wl~", "~fl~", "~eh~"])
    {
      if (!(specialSubscription in this.knownSubscriptions))
      {
        let subscription = Subscription.fromURL(specialSubscription);
        if (subscription)
          this.addSubscription(subscription, true);
      }
    }

    if (userFilters)
    {
      for each (let filter in userFilters)
      {
        filter = Filter.fromText(filter);
        if (filter)
          this.addFilter(filter, true);
      }
    }

    timeLine.log("loaded from disk");
    this.triggerSubscriptionObservers("reload", this.subscriptions);
    timeLine.log("reload subscription observers");
  },

  /**
   * Parses filter data from a stream. If the data contains user filters outside of filter
   * groups (Adblock Plus 0.7.x data) these filters are returned - they need to be added
   * separately.
   */
  parseIniFile: function(/**nsIUnicharLineInputStream*/ stream) /**Array of String*/
  {
    let wantObj = true;
    this.fileProperties = {};
    let curObj = this.fileProperties;
    let curSection = null;
    let line = {};
    let haveMore = true;
    let userFilters = null;
    while (true)
    {
      if (haveMore)
        haveMore = stream.readLine(line);
      else
        line.value = "[end]";

      let val = line.value;
      if (wantObj === true && /^(\w+)=(.*)$/.test(val))
        curObj[RegExp.$1] = RegExp.$2;
      else if (/^\s*\[(.+)\]\s*$/.test(val))
      {
        let newSection = RegExp.$1.toLowerCase();
        if (curObj)
        {
          // Process current object before going to next section
          switch (curSection)
          {
            case "filter":
            case "pattern":
              Filter.fromObject(curObj);
              break;
            case "subscription":
              let subscription = Subscription.fromObject(curObj);
              if (subscription)
                this.addSubscription(subscription, true);
              break;
            case "subscription filters":
            case "subscription patterns":
              if (this.subscriptions.length)
              {
                let subscription = this.subscriptions[this.subscriptions.length - 1];
                for each (let text in curObj)
                {
                  let filter = Filter.fromText(text);
                  if (filter)
                  {
                    subscription.filters.push(filter);
                    filter.subscriptions.push(subscription);
                  }
                }
              }
              break;
            case "user patterns":
              userFilters = curObj;
              break;
          }
        }

        if (newSection == 'end')
          break;

        curSection = newSection;
        switch (curSection)
        {
          case "filter":
          case "pattern":
          case "subscription":
            wantObj = true;
            curObj = {};
            break;
          case "subscription filters":
          case "subscription patterns":
          case "user patterns":
            wantObj = false;
            curObj = [];
            break;
          default:
            wantObj = undefined;
            curObj = null;
        }
      }
      else if (wantObj === false && val)
        curObj.push(val.replace(/\\\[/g, "["));
    }
    return userFilters;
  },

  /**
   * Saves all subscriptions back to disk
   */
  saveToDisk: function()
  {
    if (!this.file)
      return;

    try {
      this.file.normalize();
    } catch (e) {}

    // Make sure the file's parent directory exists
    try {
      this.file.parent.create(this.file.DIRECTORY_TYPE, 0755);
    } catch (e) {}

    let tempFile = this.file.clone();
    tempFile.leafName += "-temp";
    let stream;
    try {
      let fileStream = Components.classes["@mozilla.org/network/file-output-stream;1"]
                                 .createInstance(Components.interfaces.nsIFileOutputStream);
      fileStream.init(tempFile, 0x02 | 0x08 | 0x20, 0644, 0);

      stream = Components.classes["@mozilla.org/intl/converter-output-stream;1"]
                         .createInstance(Components.interfaces.nsIConverterOutputStream);
      stream.init(fileStream, "UTF-8", 16384, Components.interfaces.nsIConverterInputStream.DEFAULT_REPLACEMENT_CHARACTER);
    }
    catch (e) {
      dump("Adblock Plus: failed to create file " + tempFile.path + ": " + e + "\n");
      return;
    }

    const maxBufLength = 1024;
    let buf = ["# Adblock Plus preferences", "version=" + this.formatVersion];
    let lineBreak = abp.getLineBreak();
    function writeBuffer()
    {
      try {
        stream.writeString(buf.join(lineBreak) + lineBreak);
        buf = [];
        return true;
      }
      catch (e) {
        stream.close();
        dump("Adblock Plus: failed to write to file " + tempFile.path + ": " + e + "\n");
        try {
          tempFile.remove(false);
        }
        catch (e2) {}
        return false;
      }
    }

    let saved = {__proto__: null};

    // Save filter data
    for each (let subscription in this.subscriptions)
    {
      for each (let filter in subscription.filters)
      {
        if (!(filter.text in saved))
        {
          filter.serialize(buf);
          saved[filter.text] = filter;

          if (buf.length > maxBufLength && !writeBuffer())
            return;
        }
      }
    }

    // Save subscriptions
    for each (let subscription in this.subscriptions)
    {
      buf.push("");
      subscription.serialize(buf);
      if (subscription.filters.length)
      {
        buf.push("", "[Subscription filters]")
        subscription.serializeFilters(buf);
      }

      if (buf.length > maxBufLength && !writeBuffer())
        return;
    }

    try {
      stream.writeString(buf.join(lineBreak) + lineBreak);
      stream.close();
    }
    catch (e) {
      dump("Adblock Plus: failed to close file " + tempFile.path + ": " + e + "\n");
      try {
        tempFile.remove(false);
      }
      catch (e2) {}
      return;
    }

    if (this.file.exists()) {
      // Check whether we need to backup the file
      let part1 = this.file.leafName;
      let part2 = "";
      if (/^(.*)(\.\w+)$/.test(part1))
      {
        part1 = RegExp.$1;
        part2 = RegExp.$2;
      }

      let doBackup = (prefs.patternsbackups > 0);
      if (doBackup)
      {
        let lastBackup = this.file.clone();
        lastBackup.leafName = part1 + "-backup1" + part2;
        if (lastBackup.exists() && (Date.now() - lastBackup.lastModifiedTime) / 3600000 < prefs.patternsbackupinterval)
          doBackup = false;
      }

      if (doBackup)
      {
        let backupFile = this.file.clone();
        backupFile.leafName = part1 + "-backup" + prefs.patternsbackups + part2;

        // Remove oldest backup
        try {
          backupFile.remove(false);
        } catch (e) {}

        // Rename backup files
        for (let i = prefs.patternsbackups - 1; i >= 0; i--) {
          backupFile.leafName = part1 + (i > 0 ? "-backup" + i : "") + part2;
          try {
            backupFile.moveTo(backupFile.parent, part1 + "-backup" + (i+1) + part2);
          } catch (e) {}
        }
      }
    }

    tempFile.moveTo(this.file.parent, this.file.leafName);
  }
};
abp.filterStorage = filterStorage;
