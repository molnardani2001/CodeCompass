require([
    'dojo/_base/declare',
    'dojo/dom-class',
    'dojo/dom-construct',
    'dojo/topic',
    'dojo/dom-style',
    'dijit/TitlePane',
    'dijit/layout/ContentPane',
    'dijit/layout/TabContainer',
    'dijit/form/Button',
    'codecompass/model',
    'codecompass/viewHandler',
    'codecompass/urlHandler',
    'dojox/highlight',
    'dojox/highlight/languages/_all'],
  function (declare, domClass, dom, topic, style, TitlePane, ContentPane, TabContainer, Button,
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

      createDiffTable : function (pullfiles) {
        this._pullfiles = pullfiles;

        dom.empty(this._diff.domNode);
        let that = this;

        that._diff._header = new ContentPane();
        that._diff.addChild(that._diff._header);

        that.loadDiffTable(that._pullfiles);
      },

      loadDiffTable : function (pullfiles) {
        this._files = {};

        let that = this;
        dom.empty(that._diff._header.domNode);

        pullfiles.forEach( function (pullfile) {
          let commentCount = model.githubservice.getCommentCountForPullFile(that._pullId, pullfile.path);

          that._files[pullfile.path] = new TitlePane({
            title        : pullfile.path + ((commentCount != 0) ? (' (comments: ' + commentCount + ')') : ''),
            open         : false,
            content      : new ContentPane()
          });

          that._header.addChild(that._files[pullfile.path]);

          let changes = that.parseSingleDiffFile(pullfile);
          if (changes != null)
          {
            console.log('Step 3/3 - ' + pullfile.path + ' - adding diff title pane as child');
            that._files[pullfile.path].content.addChild(changes);
          }
          else {
            dom.create('div', {
              style : 'font-size: 1em;',
              innerHTML: 'Diff not supported for this many changes'
            }, that._files[pullfile.path].content.domNode);
          }

          let comments = that.getCommentsForPullFile(pullfile);
          if (comments != null)
          {
            console.log('Step 2/2 - ' + pullfile.path + ' - adding comments title pane as child');
            that._files[pullfile.path].content.addChild(comments);
          }
        })
      },

      getCommentsForPullFile : function (pullfile)  {
        let that = this;

        let commentCount = model.githubservice.getCommentCountForPullFile(that._pullId, pullfile.path);
        if (0 == commentCount) return;

        let commentsInfo = model.githubservice.getCommentsForPullFilePerDiffHunk(that._pullId, pullfile.path);
        let comments = dom.create('table', {
          style : 'padding: 20px;' +
            'border-radius: 3px;' +
            'border-collapse: collapse;' +
            'width: 100%;'
        });
        console.log('Step 1/2 - ' + pullfile.path + ' - creating comments title pane');
        let ret = new TitlePane({
          title        : 'Comments for: ' + pullfile.path,
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

      parseSingleDiffFile : function (pullfile) {
        let that = this;
        if (!pullfile.patch)
        {
          return;
        }

        var lines = pullfile.patch.split(/\r?\n/);

        var fileDiffDom = dom.create('table', {
          style : 'font-size: 10pt;' +
            'border-collapse: collapse;' +
            'width: 100%;'
        });
        console.log('Step 1/3 - ' + pullfile.path + ' - creating diff title pane');
        let ret = new TitlePane({
          title        : 'Current changes for: ' + pullfile.path,
          open         : false,
          content      : fileDiffDom
        });

        for (let i = 0; i < lines.length; ++i) {
          var line = lines[i];
          console.log(line);
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
            console.log('Step 1.5/3 - ' + pullfile.path + ' - parsed diffHunk');
          }
        }
        console.log('Step 2/3 - ' + pullfile.path + ' - parsed all diffHunks');
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

      init : function (pull, pullfiles) {
        this.createHeader(pull);
        this.createDiffTable(pullfiles);
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

      loadPullFileView : function (pull, pullfiles) {
        this._info.init(pull, pullfiles);
      },

      setState : function (state) {
        if (state.center !== this.id || !state.pull)
          return;
        this.loadPullFileView(state.pull, state.pullfiles);
      },

      _subscribeTopics : function () {
        var that = this;
        topic.subscribe('codecompass/gitHubPullFileView', function (message) {
          that.loadPullFileView(message.pull, message.pullfiles);
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