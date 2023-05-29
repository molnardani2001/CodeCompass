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

    var GitHubPullInfo = declare(ContentPane, {
        constructor : function () {
            this._header = new ContentPane();
        },

        postCreate : function () {
            this.addChild(this._header);
        },

        createHeader : function (pullId, labels) {
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

            //labels
            labels.forEach(
                function(node) {
                    dom.create('div',
                        {
                            style: 'background: white;' +
                              'border: 1px solid #c5d5dd;' +
                              'color: black;' +
                              'padding: 7px;' +
                              'border-radius: 40px;' +
                              'float: right;' +
                              'font-size: 1.3em;',
                            innerHTML: node.name
                        }, header);
                }
            )

            //title
            dom.create('div',
                {
                    style: 'font-weight: 600;' +
                      'font-size: 2em;' +
                      'padding-bottom: 15px',
                    innerHTML: '#' + pull.number + ': ' + pull.title
                }, header);

            //body
            var body = dom.create('div',
                {
                    style: 'background: #335180;' +
                        'border: 1px solid #c5d5dd;' +
                        'border-radius: 3px;' +
                        'font-size: 1.3em;' +
                        'color: white;' +
                        'padding: 10px;'
                }, header);

            //body text
            if (pull.body != 'null' && pull.body != '')
                dom.create('div',
                    {
                        style: 'font-style: italic',
                        innerHTML: pull.body
                    }, body);
            else dom.create('div',
                    {
                        innerHTML: ''
                    }, body);

            //time created
            var timeCreated = (new Date(pull.createdAt).toLocaleString());
            dom.create('div',
                {
                    style: 'float: right;',
                    innerHTML: 'created ' + timeCreated
                }, body);

            //author
            dom.create('div',
                {
                    style: 'font-weight: 600;',
                    innerHTML: 'author: ' + pull.user
                }, body)

            //meta
            var meta = dom.create('div',
                {
                    style: 'background: #6a737d;' +
                        'border: 1px solid #c5d5dd;' +
                        'color: white;' +
                        'font-size: 1.3em;' +
                        'border-radius: 3px;' +
                        'padding: 10px;'
                }, header);

            //time updated
            var timeUpdated = (new Date(pull.updatedAt).toLocaleString());
            dom.create('div',
                {
                    style: 'color: yellow;' +
                        'float: right;',
                    innerHTML: 'updated ' + timeUpdated
                }, meta);

            //head label
            dom.create('div', { innerHTML: 'head: ' + pull.headLabel }, meta);

            if (!pull.isOpen)
            {
                var timeClosed = (new Date(pull.closedAt).toLocaleString());
                dom.create('div',
                    {
                        style: 'color: yellow;' +
                            'float: right;',
                        innerHTML: 'closed ' + timeClosed
                    }, meta);
            }

            //base label
            dom.create('div', { innerHTML: 'base: ' + pull.baseLabel }, meta);

            if (pull.isMerged)
            {
                let timeMerged = (new Date(pull.mergedAt).toLocaleString());
                dom.create('div',
                    {
                        style: 'color: yellow;' +
                            'float: right;',
                        innerHTML: 'merged ' + timeMerged +
                          ' by ' + pull.mergedBy
                    }, meta);
            }

            //change sum
            dom.create('div', { innerHTML: 'additions: ' + pull.additions }, meta);
            dom.create('div', { innerHTML: 'deletions: ' + pull.deletions }, meta);
            dom.create('div', { innerHTML: 'changed files: ' + pull.changedFiles }, meta);
        },

        init : function (pullId, labels) {
            this.createHeader(pullId, labels);
        },
    });

    var GitHubPullView = declare(TabContainer, {
        constructor : function () {
            this._info = new GitHubPullInfo({
                id    : 'pullinfo',
                title : 'Pull Request Information'
            });
            this._subscribeTopics();
        },

        postCreate : function () {
            this.addChild(this._info);
        },

        loadPullView : function (pullId, labels) {
            this._info.init(pullId, labels);
        },

        setState : function (state) {
            if (state.center !== this.id || !state.pull)
                return;
            this.loadPullView(state.pull, state.labels);
        },

        _subscribeTopics : function () {
            var that = this;
            topic.subscribe('codecompass/gitHubPullView', function (message) {
                that.loadPullView(message.pull, message.labels);
                topic.publish('codecompass/setCenterModule', that.id);
                urlHandler.setStateValue({
                    center    : that.id,
                    pull      : message.pull
                });
            });
        },
    });

    var pullView = new GitHubPullView({id : 'githubpullview'});
    viewHandler.registerModule(pullView, {
        type : viewHandler.moduleType.Center
    });
});
