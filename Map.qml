import QtQuick 2.6
import QtLocation 5.3
import QtPositioning 5.0
Map {
    id: root
    Plugin {
        id: mapPlugin
        name:"esri"
        PluginParameter{name:"esri.mapping.cache.disk.cost_strategy";value:"unitary";}  //use number of tile
        PluginParameter{name:"esri.mapping.cache.disk.size";value:"1000000";}           //use maximumm 1000000 tile
    }
    zoomLevel: maximumZoomLevel
    plugin: mapPlugin
    activeMapType: supportedMapTypes[1]
    color: "black"
    copyrightsVisible: false
    onBearingChanged: {
        root.bearing = 0;
    }
    onTiltChanged: {
        root.tilt = 0;
    }
    MapPolygon {
        id: fov
        color: "transparent"
        border.width: 2
        border.color: "yellow"
        path: [
            {latitude: 0, longitude: 0},
            {latitude: 0, longitude: 0},
            {latitude: 0, longitude: 0},
            {latitude: 0, longitude: 0}
        ]
    }
    MapPolyline {
        id: los
        line.width: 2
        line.color: "yellow"
        path: [
            {latitude: 0, longitude: 0},
            {latitude: 0, longitude: 0}
        ]
    }

    MapQuickItem {
        id: track
        sourceItem: Rectangle {
            color: "red"
            width: 10
            height: 10
            opacity: 0.5
        }
    }

    MapQuickItem {
        id: plane

        anchorPoint: Qt.point(sourceItem.width / 2,sourceItem.height / 2)
        sourceItem: Column {
            width: 50
            height: 50
            Rectangle {
                color: "red"
                anchors.horizontalCenter: parent.horizontalCenter
                width: 10
                height: 10
            }
            Rectangle {
                color: "red"
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width
                height: 10
            }
            Rectangle {
                color: "red"
                anchors.horizontalCenter: parent.horizontalCenter
                width: 10
                height: 10
            }
            Rectangle {
                color: "red"
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width / 2
                height: 10
            }
        }
    }

    function updateData(data)
    {
        plane.coordinate = data["coordinate"]
        plane.rotation = data["yaw"]
        track.coordinate =  data["targetCoordinate"]
        var losPath = los.path;
        losPath[0] = data["coordinate"];
        losPath[1] = data["centerCoordinate"];
        los.path = losPath;

    }

    function centerMap()
    {
        root.center = plane.coordinate;
    }
}
