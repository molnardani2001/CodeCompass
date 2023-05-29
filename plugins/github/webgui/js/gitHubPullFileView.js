require([
    'dojo/_base/declare',
    'dojo/dom-class',
    'dojo/dom-construct',
    'dojo/topic',
    'dojo/dom-style',
    'dijit/TitlePane',
    'dijit/layout/ContentPane',
    'dijit/layout/TabContainer',
    'dijit/layout/BorderContainer',
    'dijit/form/Button',
    'codecompass/model',
    'codecompass/viewHandler',
    'codecompass/urlHandler',
    'dojox/highlight',
    'dojox/highlight/languages/_all'],
  function (declare, domClass, dom, topic, style, TitlePane, ContentPane, TabContainer, BorderContainer, Button,
            model, viewHandler, urlHandler, highlight) {

    model.addService('githubservice', 'GitHubService', GitHubServiceClient);

    var GitHubPullFileDiff = declare(ContentPane, {
      constructor : function () {
        this._header = new ContentPane();
        this._diff = new ContentPane();
      },

      postCreate : function () {
        this.addChild(this._header);
        this.addChild(this._diff);
      },

      createHeader : function (pullId) {
        this._pullId = pullId;
        dom.empty(this._header.domNode);
        var pull = model.githubservice.getPull(pullId);

        //header
        var header = dom.create('div',
          {
            style: 'background: #335180;' +
              'border: 1px solid #c5d5dd;' +
              'border-radius: 3px;' +
              'color: white;' +
              'padding: 10px;'
          }, this._header.domNode);

        //title
        dom.create('div',
          {
            style: 'font-weight: 600;' +
              'font-size: 2em;',
            innerHTML: '#' + pull.number + ': ' + pull.title
          }, header);

        //time created
        let timeCreated = (new Date(pull.createdAt).toLocaleString());
        dom.create('div',
          {
            style: 'float: right;' +
              'padding-right: 5px;',
            innerHTML: 'created ' + timeCreated
          }, header);

        //author
        dom.create('div',
          {
            style: 'font-weight: 600;',
            innerHTML: 'author: ' + pull.user
          }, header)
      },

      createDiffTable : function (pullFiles) {
        this._pullFiles = pullFiles;

        dom.empty(this._diff.domNode);
        let that = this;

        that._diff._header = new ContentPane();
        that._diff.addChild(that._diff._header);

        that.loadDiffTable(that._pullFiles);
      },

      loadDiffTable : function (pullFiles) {
        this._files = {};

        let that = this;
        dom.empty(that._diff._header.domNode);

        pullFiles.forEach( function (pullFile) {
          let commentCount = model.githubservice.getCommentCountForPullFile(that._pullId, pullFile.path);

          that._files[pullFile.path] = new TitlePane({
            title        : pullFile.path + ((commentCount != 0) ? (' (comments: ' + commentCount + ')') : ''),
            open         : false,
            content      : new BorderContainer()
          });

          that._header.addChild(that._files[pullFile.path]);

          let changes = that.parseSingleDiffFile(pullFile);
          if (changes != null)
          {
            that._files[pullFile.path].content.addChild(changes);

            let comments = that.getCommentsForPullFile(pullFile);
            if (comments != null)
            {
              that._files[pullFile.path].content.addChild(comments);
            }
          }
          else {
            dom.create('div', {
              style: 'font-size: 1em;',
              innerHTML: 'Diff not supported'
            }, that._files[pullFile.path].content.domNode);
          }
        })
      },

      getCommentsForPullFile : function (pullFile)  {
        let that = this;

        let commentCount = model.githubservice.getCommentCountForPullFile(that._pullId, pullFile.path);
        if (0 == commentCount) return;

        let commentsInfo = model.githubservice.getCommentsForPullFilePerDiffHunk(that._pullId, pullFile.path);
        let comments = dom.create('table', {
          style : 'padding: 20px;' +
            'border-radius: 3px;' +
            'border-collapse: collapse;' +
            'width: 100%;'
        });
        let ret = new TitlePane({
          title        : 'Comments for: ' + pullFile.path,
          open         : false,
          content      : comments
        });

        commentsInfo.forEach(function(commentsByDiffHunk) {
          let lines = commentsByDiffHunk.key.split(/\r?\n/);
          let line = lines[0];

          //diffHunk
          let lineContainerDiff = dom.create('tr', {
            style: 'white-space: pre-wrap;'
          }, comments);
          dom.create('td', {
            colspan: "100%",
            class: 'diff-linetext diff-rowheader',
            innerHTML: line
          }, lineContainerDiff);

          let res = line.match(/[0-9]+/g);
          that.parseSingleDiffSection(lines, 1, comments,
          parseInt(res[0]), parseInt(res[2]));

          //comments
          commentsByDiffHunk.value.forEach(function (comment) {
            let commentTr = dom.create('tr', {}, comments);
            let commentTd = dom.create('td', {colspan : "100%"}, commentTr);
            let commentContent = dom.create('div', {
              style: 'background: white;' +
                'border: 1px solid black;' +
                'padding: 10px;' +
                'border-radius: 40px;',
              innerHTML :
              comment.body
            }, commentTd);
            let timeCreated = (new Date(comment.createdAt).toLocaleString());
            dom.create('div', {
              style     :
                'font-style: italic',
              innerHTML :
                'by ' + comment.user +
                ' on ' + timeCreated + ((comment.commitId != comment.originalCommitId) ? (' (comment is outdated)') : '')
            }, commentContent)
          });
        });
        return ret;
      },

      parseSingleDiffFile : function (pullFile) {
        let that = this;
        if (!pullFile.patch)
        {
          return;
        }

        var lines = pullFile.patch.split(/\r?\n/);

        var fileDiffDom = dom.create('table', {
          style : 'font-size: 10pt;' +
            'border-collapse: collapse;' +
            'width: 100%;'
        });
        let ret = new TitlePane({
          title        : 'Current changes for: ' + pullFile.path,
          open         : false,
          content      : fileDiffDom
        });

        for (let i = 0; i < lines.length; ++i) {
          var line = lines[i];
          if ('@' === line[0]) {
            //diffHunk
            var lineContainer = dom.create('tr', {
              style : 'white-space: pre-wrap;'
            }, fileDiffDom);

            dom.create('td', {
              colspan   : "100%",
              class     : 'diff-linetext diff-rowheader',
              innerHTML : line
            }, lineContainer);

            //diff
            var res = line.match(/[0-9]+/g);
            i = this.parseSingleDiffSection(lines, i + 1, fileDiffDom,
              parseInt(res[0]), parseInt(res[2])) - 1;
          }
        }
        return ret;
      },

      parseSingleDiffSection : function (lines, i, fileDiffDom, oldlinenumber, newlinenumber) {
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
            class: 'diff-linecontainer'
          }, fileDiffDom);

          dom.create('td', {
            class: 'diff-linenumber ' +
              (mode === 'old' && oldlinenumber ? 'old' :
                mode === 'new' && newlinenumber ? 'new' : ''),
            innerHTML: ('old' == mode || 'both' == mode
              ? oldlinenumber : '&nbsp;')
          }, lineContainer);

          dom.create('td', {
            class: 'diff-linenumber ' + (mode === 'new' && newlinenumber ? 'new' :
              mode === 'old' && oldlinenumber ? 'old' : ''),
            innerHTML: ('new' == mode || 'both' == mode
              ? newlinenumber : '&nbsp;')
          }, lineContainer);

          dom.create('td', {
            class: 'diff-linetext ' + mode,
            innerHTML: content
          }, lineContainer);
        }
        return i;
      },

      init : function (pullId, pullFiles) {
        this.createHeader(pullId);
        this.createDiffTable(pullFiles);
      },
    });

    var GitHubPullFileView = declare(TabContainer, {
      constructor : function () {
        this._info = new GitHubPullFileDiff({
          id    : 'github-pullfilediff',
          title : 'Pull Request File Visual Diff'
        });
        this._subscribeTopics();
      },

      postCreate : function () {
        this.addChild(this._info);
      },

      loadPullFileView : function (pull, pullFiles) {
        this._info.init(pull, pullFiles);
      },

      setState : function (state) {
        if (state.center !== this.id || !state.pull)
          return;
        this.loadPullFileView(state.pull, state.pullFiles);
      },

      _subscribeTopics : function () {
        var that = this;
        topic.subscribe('codecompass/gitHubPullFileView', function (message) {
          that.loadPullFileView(message.pull, message.pullFiles);
          topic.publish('codecompass/setCenterModule', that.id);
          urlHandler.setStateValue({
            center    : that.id,
            pull      : message.pull
          });
        });
      },
    });

    var pullFileView = new GitHubPullFileView({id : 'githubpullfileview'});
    viewHandler.registerModule(pullFileView, {
      type : viewHandler.moduleType.Center
    });
  });