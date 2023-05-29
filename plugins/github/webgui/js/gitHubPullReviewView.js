require([
        'dojo/_base/declare',
        'dojo/dom-construct',
        'dojo/topic',
        'dojo/dom-style',
        'dijit/layout/ContentPane',
        'dijit/layout/TabContainer',
        'codecompass/model',
        'codecompass/viewHandler',
        'codecompass/urlHandler'],
function (declare, dom, topic, style, ContentPane, TabContainer,
          model, viewHandler, urlHandler) {

    model.addService('githubservice', 'GitHubService', GitHubServiceClient);

    var GitHubPullReviewInfo = declare(ContentPane, {
        constructor : function () {
            this._header = new ContentPane();
        },

        postCreate : function () {
            this.addChild(this._header);
        },

        createHeader : function (pullId) {
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

          //reviews
          let reviewsAll = model.githubservice.getContributionsForPull(pull.number);
          if (reviewsAll.length != 0)
          {
            dom.create('div',
              {
                style: 'font-weight: 600;' +
                  'font-size: 1.5em;' +
                  'padding-top: 15px',
                innerHTML: 'Summary of reviews'
              }, header);
            var reviews = dom.create('div',
              {
                style: 'background: #6a737d;' +
                  'border: 1px solid #c5d5dd;' +
                  'color: white;' +
                  'padding: 5px;' +
                  'border-radius: 3px;'
              }, header);

            reviewsAll.forEach(function (reviewSum){
              dom.create('div',
                {
                  innerHTML: reviewSum.comments + ' comment' +
                    ((1 == reviewSum.comments) ? ', ' : 's, ') +
                    reviewSum.changeRequests + ' change request' +
                    ((1 == reviewSum.changeRequests) ? ', ' : 's, ') +
                    reviewSum.approves + ' approve' +
                    ((1 == reviewSum.approves) ? ' ' : 's ') +
                    'made by ' + reviewSum.user
                }, reviews);
            });
          }

          //comments
          dom.create('div',
            {
              style: 'font-weight: 600;' +
                'font-size: 1.5em;' +
                'padding-top: 15px',
              innerHTML: 'Summary of comments per file'
            }, header);
          model.githubservice.getFileListForPull(pull.number).forEach(function (file) {
            let commentsInfo = model.githubservice.getCommentCountForPullFilePerAuthor(pull.number, file.path);
            //comments
            let comments = dom.create('div',
              {
                style: 'background: #6a737d;' +
                  'border: 1px solid #c5d5dd;' +
                  'color: white;' +
                  'padding: 5px;' +
                  'border-radius: 3px;'
              }, header);
            dom.create('div',
              {
                style: 'font-size: 1.3em;' +
                  'font-weight: 600;',
                innerHTML: file.path
              }, comments);
            commentsInfo.forEach(function (commentInfo) {
              dom.create('div',
                {
                  innerHTML: commentInfo.key + ' made ' + commentInfo.value + ' comment' +
                    ((1 == commentInfo.value) ? '' : 's')
                }, comments);
            });
          });
        },

        init : function (pullId) {
            this.createHeader(pullId);
        },
    });

    var GitHubPullReviewView = declare(TabContainer, {
        constructor : function () {
            this._info = new GitHubPullReviewInfo({
                id    : 'pullinfo-review',
                title : 'Pull Request Reviews'
            });
            this._subscribeTopics();
        },

        postCreate : function () {
            this.addChild(this._info);
        },

        loadPullReviewView : function (pullId) {
            this._info.init(pullId);
        },

        setState : function (state) {
            if (state.center !== this.id || !state.pull)
                return;
            this.loadPullReviewView(state.pull);
        },

        _subscribeTopics : function () {
            var that = this;
            topic.subscribe('codecompass/gitHubPullReviewView', function (message) {
                that.loadPullReviewView(message.pull);
                topic.publish('codecompass/setCenterModule', that.id);
                urlHandler.setStateValue({
                    center    : that.id,
                    pull      : message.pull
                });
            });
        },
    });

    var pullReviewView = new GitHubPullReviewView({id : 'githubpullreviewview'});
    viewHandler.registerModule(pullReviewView, {
        type : viewHandler.moduleType.Center
    });

});
