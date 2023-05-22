define([
    'dojo/_base/declare',
    'dojo/dom-class',
    'dojo/dom-construct',
    'dijit/TitlePane',
    'dijit/layout/ContentPane',
    'dijit/form/Button',
    'codecompass/model',
    'dojox/highlight',
    'dojox/highlight/languages/cpp',],
  function (declare, domClass, dom, TitlePane, ContentPane, Button, model,
            highlight) {

    model.addService('githubservice', 'GitHubService', GitHubServiceClient);

    var GitHubPullFileDiffViewer = declare(ContentPane, {
      _contextLines : 3,
      _contextLineStep : 5,
      _titlePanelCache : {},

      _createExpanderBtn : function (refNode, path, sideBySide) {
        var that = this;

        var expanderBox = dom.create('th', {
          colspan : sideBySide ? 1 : 2,
          class   : 'diff-expander'
        }, refNode);

        var expandBtn = new Button({
          class     : 'github-expander-btn',
          iconClass : 'icon icon-expand',
          showLabel : false,
          onClick   : function () {
            var titlePanel = that._titlePanelCache[path];
            titlePanel.contextLines += that._contextLineStep;
            titlePanel.set('content',
              that.getFileDiff(path, titlePanel.contextLines));
          }
        }, dom.create('div', {}, expanderBox));
      },

      parseSingleDiffFile : function (pullfile, sideBySide) {
        var lines = pullfile.patch.split(/\r?\n/);

        var fileDiffDom = dom.create('table', {
          style : 'font-size: 10pt;' +
            'border-collapse: collapse;' +
            'width: 100%;'
        });

        for (i = 0; i < lines.length; ++i) {
          var line = lines[i];

          if ('@' === line[0]) {
            var lineContainer = dom.create('tr', {
              style : 'white-space: pre-wrap;'
            }, fileDiffDom);

            //--- Create expander button in the diff header ---//

            //this._createExpanderBtn(lineContainer, pullfiles, sideBySide);

            //--- Create line diff header ---//

            dom.create('td', {
              colspan   : sideBySide ? 3 : 1,
              class     : 'diff-linetext diff-rowheader',
              innerHTML : line
            }, lineContainer);

            var res = line.match(/[0-9]+/g);

            i = this.parseSingleDiffSection(lines, i + 1, fileDiffDom,
              parseInt(res[0]), parseInt(res[2]), sideBySide) - 1;
          } else if (0 === line.length) {
            return fileDiffDom;
          } else {
            console.log('Error at line ' + i + ': ' + line);
            throw 'Error in _parseSingleDiffFile';
          }
        }
        return fileDiffDom;
      },

      parseSingleDiffSection : function (lines, i, fileDiffDom, oldlinenumber,
                                         newlinenumber, sideBySide) {
        --oldlinenumber;
        --newlinenumber;

        for (; i < lines.length; ++i) {
          var line = lines[i];

          var content = line;
          var mode = 'nomode';

          if (0 === line.length)
            return i;

          switch (line[0]) {
            case '+':
              ++newlinenumber;
              content = content.substr(1);
              mode = 'new';
              break;

            case '-':
              ++oldlinenumber;
              content = content.substr(1);
              mode = 'old';
              break;

            case ' ':
              ++newlinenumber;
              ++oldlinenumber;
              content = content.substr(1);
              mode = 'both';
              break;

            case '\\':
              content = '(' + content.substr(2) + ')';
              mode = 'remark';
              break;

            case '@':
            case 'd':
              return i;

            default:
              console.log(
                'Error at line ' + i + ' of ' + lines.length + ': ' + line);
              throw 'Error in _parseSingleDiffSection';
          }

          content = highlight.processString(content).result;

          var lineContainer = dom.create('tr', {
            class : 'diff-linecontainer'
          }, fileDiffDom);

          dom.create('td', {
            class : 'diff-linenumber ' +
              (mode === 'old' && oldlinenumber ? 'old' :
                !sideBySide && mode === 'new' && newlinenumber ? 'new' : ''),
            innerHTML : ('old' == mode || 'both' == mode
              ? oldlinenumber : '&nbsp;')
          }, lineContainer);

          if (sideBySide)
            dom.create('td', {
              class : 'diff-linetext diff-linetext-sidebyside-side ' + (
                mode === 'old' && oldlinenumber ? mode : ''),
              innerHTML : ('old' == mode || 'both' == mode || 'remark' == mode
                ? content : '&nbsp;')
            }, lineContainer);

          dom.create('td', {
            class : 'diff-linenumber ' + (mode === 'new' && newlinenumber ? 'new' :
              !sideBySide && mode === 'old' && oldlinenumber ? 'old' : ''),
            innerHTML : ('new' == mode || 'both' == mode
              ? newlinenumber : '&nbsp;')
          }, lineContainer);

          if (sideBySide)
            dom.create('td', {
              class : 'diff-linetext diff-linetext-sidebyside-side ' + (
                mode === 'new' && newlinenumber ? mode : ''),
              innerHTML : ('new' == mode || 'both' == mode || 'remark' == mode
                ? content : '&nbsp;')
            }, lineContainer);
          else
            dom.create('td', {
              class: 'diff-linetext ' + mode,
              innerHTML: content
            }, lineContainer);
        }

        return i;
      },

      load : function (pullfiles, sideBySide) {
        this._titlePanelCache = {};
        dom.empty(this.domNode);

        //var that = this;
        this._pullfiles = pullfiles;
        this._sideBySide = sideBySide;

        pullfiles.forEach(function (pullfile) {
            this._titlePanelCache[pullfile.path] = new TitlePane({
              title        : pullfile.path,
              open         : true,
              contextLines : this._contextLines,
              content      : this.parseSingleDiffFile(pullfile/*, this._contextLines*/, this._sideBySide)
            });

            this.addChild(this._titlePanelCache[pull.path]);
        })
      }
    });

    return declare(ContentPane, {
      _sideBySide : false,

      constructor : function () {
        var that = this;

        this._header = new ContentPane();

        this._inlineBtn = new Button({
          label   : 'Inline',
          class   : 'inline-btn',
          onClick : function () {
            if (that._sideBySide) {
              that._sideBySide = false;
              that.changeSplitButtonType();
              that.loadDiffTable(that._pullfiles);
            }
          }
        });

        this._sideBySideBtn = new Button({
          label   : 'Side-By-Side',
          class   : 'side-by-side-btn',
          onClick : function () {
            if (!that._sideBySide) {
              that._sideBySide = true;
              that.changeSplitButtonType();
              that.loadDiffTable(that._pullfiles);
            }
          }
        });

        this._diffTable = new GitHubPullFileDiffViewer();
      },

      postCreate : function () {
        this.addChild(this._header);
        this._header.addChild(this._inlineBtn);
        this._header.addChild(this._sideBySideBtn);
        this.addChild(this._diffTable);

        this.changeSplitButtonType();
      },

      changeSplitButtonType : function () {
        if (this._sideBySide) {
          domClass.add(this._sideBySideBtn.domNode, 'selected');
          domClass.remove(this._inlineBtn.domNode, 'selected');
        } else {
          domClass.add(this._inlineBtn.domNode, 'selected');
          domClass.remove(this._sideBySideBtn.domNode, 'selected');
        }
      },

      loadDiffTable : function (pullfiles) {
        this._pullfiles = pullfiles;

        this._diffTable.load(pullfiles, this._sideBySide);
      }
    });
  });
