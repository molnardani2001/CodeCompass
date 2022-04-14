require([
        'dojo/_base/declare',
        'dojo/dom-construct',
        'dojo/topic',
        'dojo/dom-style',
        'dijit/layout/ContentPane',
        'dijit/layout/TabContainer',
        'codecompass/model',
        'codecompass/viewHandler',
        'codecompass/urlHandler',
        'codecompass/util'],
function (declare, dom, topic, style, ContentPane, TabContainer,
      model, viewHandler, urlHandler, util) {

    model.addService('githubservice', 'GitHubService', GitHubServiceClient);


    var GitHubPullView = declare(ContentPane, {
        constructor : function () {
            var that = this;
            this._header = new ContentPane();
            this.createHeader();
        },

        createHeader : function () {
            console.log("TESZT3");
            dom.empty(this._header.domNode);
            var header = dom.create('div', { class : 'header'}, this._header.domNode);
            dom.create(
                'div', { class : 'message', innerHTML: 'random' }, header);
        },

        setState : function (state) {
            console.log("setstate");
            return;
        }

    });

    var pullView = new GitHubPullView({id : 'githubpullview'});
    viewHandler.registerModule(pullView, {
        type : viewHandler.moduleType.Center
    });
});
