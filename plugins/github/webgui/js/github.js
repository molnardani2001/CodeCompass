require([
    'dijit/Tooltip',
    'dijit/tree/ObjectStoreModel',
    'dijit/_base/wai',
    'dojo/_base/declare',
    'dojo/store/Memory',
    'dojo/store/Observable',
    'dojo/topic',
    'codecompass/view/component/HtmlTree',
    'codecompass/model',
    'codecompass/viewHandler'],
function (Tooltip, ObjectStoreModel, wai, declare, Memory, Observable, topic,
    HtmlTree, model, viewHandler) {

    model.addService('githubservice', 'GitHubService', GitHubServiceClient);

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
                name        : 'GitHub Info',
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
                parent          : 'root',
                getChildren     : function() {
                    return that.getPulls();
                }
            });

            that._store.put({
                id              : 'contributors',
                name            : 'Contributors',
                hasChildren     : true,
                parent          : 'root',
                getChildren     : function() {
                    return that.getContributors();
                }
            });
        },

        setState : function (state) {},

        onClick : function (item, node, event) {
            if (item.onClick && !node.isExpanded)
            {
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
                    onClick         : function (item, node, event) {
                        topic.publish('codecompass/gitHubPullView', {
                            center  : 'githubpullview',
                            pull    : pull.number,
                            labels  : that.getLabelsForPull(pull)
                        });
                    },
                    getChildren     : function() {
                        var subret = [];

                        subret.push({
                            id            : pull.number + '_files',
                            name          : 'Files',
                            hasChildren   : true,
                            onClick         : function (item, node, event) {
                                topic.publish('codecompass/gitHubPullFileView', {
                                    center      : 'githubpullfileview',
                                    pull        : pull.number,
                                    pullfiles   : model.githubservice.getFileListForPull(pull.number)
                                });
                            },
                            getChildren   : function() {
                                return that.getFilesForPull(pull);
                            }
                        });

                        subret.push({
                            id            : pull.number + '_reviewers',
                            name          : 'Reviewers',
                            hasChildren   : true,
                            onClick         : function (item, node, event) {
                                topic.publish('codecompass/gitHubPullReviewView', {
                                    center  : 'githubpullreviewview',
                                    pull    : pull.number
                                });
                            },
                            getChildren   : function() {
                                return that.getReviewersForPull(pull);
                            }
                        });

                        return subret;
                    }
                });
            });
            return ret;
        },

        getFilesForPull : function (pull) {
            var ret = [];
            model.githubservice.getFileListForPull(pull.number).forEach(function (file) {
                ret.push({
                    id      : file.sha,
                    name    : file.path
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
                    id              : user.username + '_reviewer',
                    name            : user.username,
                    onClick         : function (item, node, event) {
                        topic.publish('codecompass/gitHubReviewedView', {
                            center  : 'githubreviewedview',
                            user    : user.username,
                            pulls   : model.githubservice.getPullListForReviewer(user.username)
                        });
                    },
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
                    getChildren     : function() {
                        var subret = [];

                        subret.push({
                            id              : user.username + '_authored_pulls',
                            name            : 'Pull Requests',
                            hasChildren     : true,
                            onClick         : function (item, node, event) {
                                topic.publish('codecompass/gitHubOwnedView', {
                                    center  : 'githubownedview',
                                    user    : user.username,
                                    pulls   : model.githubservice.getPullListForAuthor(user.username)
                                });
                            },
                            getChildren     : function() {
                                return that.getPullsForAuthor(user);
                            }
                        });

                        subret.push({
                            id            : user.username + '_reviewed_pulls',
                            name          : 'Reviewed',
                            hasChildren   : true,
                            onClick       : function (item, node, event) {
                                topic.publish('codecompass/gitHubReviewedView', {
                                    center  : 'githubreviewedview',
                                    user    : user.username,
                                    pulls   : model.githubservice.getPullListForReviewer(user.username)
                                });
                            },
                            getChildren   : function() {
                                return that.getPullsForReviewer(user);
                            }
                        });

                        return subret;
                    }
                })
            });
            return ret;
        },

        getPullsForAuthor : function (user) {
            var that = this;

            var ret = [];
            model.githubservice.getPullListForAuthor(user.username).forEach(function (pull) {
                ret.push({
                    id              : pull.number + '_author',
                    name            : '#' + pull.number + ': ' + pull.title,
                    onClick         : function (item, node, event) {
                        topic.publish('codecompass/gitHubPullView', {
                            center  : 'githubpullview',
                            pull    : pull.number,
                            labels  : that.getLabelsForPull(pull)
                        });
                    }
                });
            });
            return ret;
        },

        getPullsForReviewer : function (user) {
            var ret = [];
            model.githubservice.getPullListForReviewer(user.username).forEach(function (pull) {
                ret.push({
                    id              : pull.number + '_reviewer',
                    name            : '#' + pull.number + ': ' + pull.title,
                    onClick         : function (item, node, event) {
                        topic.publish('codecompass/gitHubPullReviewView', {
                            center  : 'githubpullreviewview',
                            pull    : pull.number
                        });
                    }
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
