// Runtime-only KWin observer. Transmits output names, never window titles or contents.
(function () {
    function report() {
        var outputs = [];
        var windows = workspace.stackingOrder;
        for (var i = 0; i < windows.length; ++i) {
            var w = windows[i];
            if (w.fullScreen && !w.minimized && w.output &&
                (w.onAllDesktops || w.desktops.indexOf(workspace.currentDesktop) !== -1) &&
                (!w.activities.length || w.activities.indexOf(workspace.currentActivity) !== -1)) {
                if (outputs.indexOf(w.output.name) === -1) outputs.push(w.output.name);
            }
        }
        callDBus("__NEXIS_SERVICE__", "/org/nexis/Compositor", "org.nexis.Compositor", "Report", JSON.stringify(outputs));
    }
    function observe(w) {
        ["fullScreenChanged", "minimizedChanged", "desktopsChanged", "activitiesChanged", "outputChanged"].forEach(function (name) {
            if (w[name]) w[name].connect(report);
        });
        report();
    }
    workspace.stackingOrder.forEach(observe);
    workspace.windowAdded.connect(observe);
    workspace.windowRemoved.connect(report);
    workspace.currentDesktopChanged.connect(report);
    if (workspace.currentActivityChanged) workspace.currentActivityChanged.connect(report);
    report();
})();
