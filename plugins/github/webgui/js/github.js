require([
    'dijit/Tooltip',
    'dijit/tree/ObjectStoreModel',
    'dojo/_base/declare',
    'dojo/store/Memory',
    'dojo/store/Observable',
    'dojo/topic',
    'codecompass/view/component/HtmlTree',
    'codecompass/model',
    'codecompass/viewHandler'],
function (Tooltip, ObjectStoreModel, declare, Memory, Observable, topic,
    HtmlTree, model, viewHandler) {

    model.addService('githubservice', 'GitHubService', GitHubServiceClient);
    console.log(model.githubservice);

    console.log(model.githubservice.getGitHubString());

    var ProjectHostingServiceNavigator = declare(HtmlTree, {
        constructor : function () {
            var that = this;

            this._data = [];

            this._store = new Observable(new Memory({
                data : this._data,
                getChildren : function (node) {
                    return node.getChildren ? node.getChildren(node) : [];
                }
            }));

            var dataModel = new ObjectStoreModel({
                store : that._store,
                query : { id : 'root' },
                mayHaveChildren : function (node) {
                    return node.hasChildren;
                }
            });

            this._data.push({
                id          : 'root',
                name        : 'root?',
                hasChildren : true,
                getChildren : function () {
                    return that._store.query({ parent : 'root' });
                }
            });

            this.set('model', dataModel);
            this.set('openOnClick', false);

            that._store.put({
                id              : 'pulls',
                name            : 'Pull Requests',
                hasChildren     : true,
                loaded          : true,
                parent          : 'root',
                getChildren     : function() {
                    return that.getPulls();
                }
            });

            that._store.put({
                id              : 'contributors',
                name            : 'Contributors',
                hasChildren     : true,
                loaded          : true,
                parent          : 'root',
                getChildren     : function() {
                    return that.getContributors();
                }
            });
        },

        onClick : function (item, node, event) {
            if (item.onClick)
            {
                console.log("TESZT");
                item.onClick(item, node, event);
            }
            if (item.hasChildren)
                this._onExpandoClick({node: node});
        },

        getPulls : function () {
            var that = this;

            var ret = [];

            model.githubservice.getPullList().forEach(function (pull) {
                ret.push({
                    id              : pull.number,
                    name            : '#' + pull.number + ': ' + pull.title,
                    hasChildren     : true,
                    loaded          : true,
                    parent          : 'pulls',
                    onClick         : function (item, node, event) {
                        console.log("TESZT2");
                        topic.publish('codecompass/githubPullView', {
                            center  : 'githubpullview'
                        })
                    },
                    getChildren     : function() {
                        var subret = [];

                        subret.push({
                            id            : pull.number + '_files',
                            name          : 'Files',
                            hasChildren   : true,
                            getChildren   : function() {
                                return that.getFilesForPull(pull);
                            }
                        });

                        subret.push({
                            id            : pull.number + '_labels',
                            name          : 'Labels',
                            hasChildren   : true,
                            getChildren   : function() {
                                return that.getLabelsForPull(pull);
                            }
                        });

                        subret.push({
                            id            : pull.number + '_reviewers',
                            name          : 'Reviewers',
                            hasChildren   : true,
                            getChildren   : function() {
                                return that.getReviewersForPull(pull);
                            }
                        });

                        return subret;
                    }
                })
            });
            return ret;
        },

        getFilesForPull : function (pull) {
            var ret = [];
            model.githubservice.getFileListForPull(pull.number).forEach(function (file) {
                ret.push({
                    id      : file.sha,
                    name    : file.path
                    // cssclass?
                });
            });
            return ret;
        },

        getLabelsForPull : function (pull) {
            var ret = [];
            model.githubservice.getLabelListForPull(pull.number).forEach(function (label) {
                ret.push({
                    id      : label.id,
                    name    : label.name
                });
            });
            return ret;
        },

        getReviewersForPull : function (pull) {
            var ret = [];
            model.githubservice.getReviewerListForPull(pull.number).forEach(function (user) {
                ret.push({
                    id      : user.username,
                    name    : user.username
                });
            });
            return ret;
        },

        getContributors : function () {
            var that = this;

            var ret = [];

            model.githubservice.getContributorList().forEach(function (user) {
                ret.push({
                    id              : user.username,
                    name            : user.username,
                    hasChildren     : true,
                    loaded          : true,
                    parent          : 'contributors',
                    getChildren     : function() {
                        var subret = [];

                        subret.push({
                            id            : user.username + '_pulls',
                            name          : 'Author of',
                            hasChildren   : true,
                            getChildren   : function() {
                                return that.getPullsForAuthor(user);
                            }
                        });

                        /*subret.push({
                            id            : user.username + '_reviewed_pulls',
                            name          : 'Reviewed',
                            hasChildren   : true,
                            getChildren   : function() {
                                return that.getPullsForReviewer(user);
                            }
                        });*/

                        return subret;
                    }
                })
            });
            return ret;
        },

        getPullsForAuthor : function (user) {
            var ret = [];
            model.githubservice.getPullListForAuthor(user.username).forEach(function (pull) {
                ret.push({
                    id              : pull.number,
                    name            : '#' + pull.number + ': ' + pull.title,
                });
            });
            return ret;
        },

        getPullsForReviewer : function (user) {
            var ret = [];
            model.githubservice.getPullListForReviewer(user.username).forEach(function (pull) {
                ret.push({
                    id              : pull.number,
                    name            : '#' + pull.number + ': ' + pull.title,
                });
            });
            return ret;
        },
    });

    var navigator = new ProjectHostingServiceNavigator({
        id    : 'hostnavigator',
        title : 'Project Hosting Service Navigator'
    });

    viewHandler.registerModule(navigator, {
        type : viewHandler.moduleType.Accordion,
        priority : 50
    });

});
