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

    var GitHubReviewedInfo = declare(ContentPane, {
      constructor : function () {
        this._header = new ContentPane();
      },

      postCreate : function () {
        this.addChild(this._header);
      },

      createHeader : function (user, pulls) {
        dom.empty(this._header.domNode);

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
              'font-size: 2em;' +
              'padding-bottom: 15px',
            innerHTML: 'Pull Requests reviewed by ' + user
          }, header);

        //reviewed pulls
        pulls.forEach(function (pull) {
          let pulls = dom.create('div',
            {
              style: 'background: #6a737d;' +
                'border: 1px solid #c5d5dd;' +
                'padding: 10px;' +
                'color: white;' +
                'border-radius: 40px;'
            }, header);

          dom.create('span',
            {
              style:'font-weight: 300;' +
                'font-size: 1.3em',
              innerHTML: '#' + pull.number + ': ' + pull.title
            }, pulls);

          //review sum
          let reviewSum = model.githubservice.getContributionsForPullByAuthor(pull.number, user);
          dom.create('div',
            {
              style: 'float: right;' +
                'background: #335180;' +
                'border: 1px solid #c5d5dd;' +
                'padding-left: 5px;' +
                'padding-right: 5px;' +
                'border-radius: 10px;' +
                'font-weight: 600;' +
                'font-size: 1.3em',
              innerHTML: reviewSum.comments + ' comment' +
                ((reviewSum.comments==1) ? ', ' : 's, ') +
                reviewSum.changeRequests + ' change request' +
                ((reviewSum.changeRequests==1) ? ', ' : 's, ') +
                reviewSum.approves + ' approve' +
                ((reviewSum.approves==1) ? '' : 's')
            }, pulls);

        });
      },

      init : function (user, pulls) {
        this.createHeader(user, pulls);
      },
    });

    var GitHubReviewedView = declare(TabContainer, {
      constructor : function () {
        this._info = new GitHubReviewedInfo({
          id    : 'reviewedinfo',
          title : 'Reviewed Pull Requests'
        });
        this._subscribeTopics();
      },

      postCreate : function () {
        this.addChild(this._info);
      },

      loadReviewedView : function (user, pulls) {
        this._info.init(user, pulls);
      },

      setState : function (state) {
        if (state.center !== this.id)
          return;
        this.loadReviewedView();
      },

      _subscribeTopics : function () {
        var that = this;
        topic.subscribe('codecompass/gitHubReviewedView', function (message) {
          that.loadReviewedView(message.user, message.pulls);
          topic.publish('codecompass/setCenterModule', that.id);
          urlHandler.setStateValue({
            center    : that.id,
            user      : message.user
          });
        });
      },
    });

    var reviewedView = new GitHubReviewedView({id : 'githubreviewedview'});
    viewHandler.registerModule(reviewedView, {
      type : viewHandler.moduleType.Center
    });

  });
