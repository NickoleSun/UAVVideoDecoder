import QtQuick 2.6
import QtLocation 5.3
import QtPositioning 5.0
Map {
    id: root
    property bool mousePressed: false
    property bool rulerAdding : false

    property var cursorPos: undefined
    property color measurelineColor: "GreenYellow"

    property var measureLineStartPos : undefined
    property var measureLineEndPos   : undefined
    property var listMeasureLine     : []
    property var buffMeasureLine     : undefined

    property var listClickPoint      : [
    ]
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
        line.width: 4
        line.color: "red"
        path: [
            {latitude: 0, longitude: 0},
            {latitude: 0, longitude: 0}
        ]
    }

    MapQuickItem {
        id: track
        anchorPoint: Qt.point(sourceItem.width / 2,sourceItem.height / 2)
        sourceItem: Rectangle {
            color: "red"
            width: 20
            height: 20
            opacity: 0.5
        }
    }

    MapPolyline {
        id: losComp
        line.width: 2
        line.color: "yellow"
        path: [
            {latitude: 0, longitude: 0},
            {latitude: 0, longitude: 0}
        ]
    }

    MapQuickItem {
        id: trackComp
        anchorPoint: Qt.point(sourceItem.width / 2,sourceItem.height / 2)
        sourceItem: Rectangle {
            color: "yellow"
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
    MouseArea {
        acceptedButtons: Qt.LeftButton|Qt.RightButton
        anchors.fill: parent

        onPressed:{
            var coordinate = root.toCoordinate(Qt.point(mouse.x,mouse.y))
            if(mouse.button === Qt.RightButton){ //press right-button
                measureLineStartPos = coordinate
                measureLineEndPos = coordinate
                rulerAdding = true
                root.gesture.enabled = false
                buffMeasureLine = createMeasureline(measureLineStartPos, measureLineEndPos)
            }

        }
        onPositionChanged: {
            var coordinate = root.toCoordinate(Qt.point(mouse.x,mouse.y))
            if(rulerAdding){
                measureLineEndPos = coordinate;
                changeMeasureLine(buffMeasureLine, measureLineStartPos, measureLineEndPos);
            }
        }
        onReleased: {
            if(rulerAdding){
                rulerAdding = false
                listMeasureLine.push(buffMeasureLine)
            }
            root.gesture.enabled = true
        }
        onClicked: {
            forceActiveFocus();
        }
    }

    MapItemView {
        id: markers
        model: root.listClickPoint
        delegate: MapQuickItem
        {
            anchorPoint.x: rectOrigin.width/2
            anchorPoint.y: rectOrigin.height/2
            coordinate: gps
            sourceItem: Rectangle{
                id: rectOrigin
                width: 20
                height: width
                opacity: 0.5
                radius: width/2
                color: "orange"
                Rectangle{
                    anchors.centerIn: parent
                    width: 4
                    height: width
                    radius: width/2
                    color: "black"
                }
            }
        }
    }


    //Ruler region
    Component
    {
        id:startMeasureLineComponent
        MapQuickItem
        {
            property color mColor: undefined
            id:originRulerItem
            anchorPoint.x: rectOrigin.width/2
            anchorPoint.y: 0

            sourceItem: Rectangle{
                id: rectOrigin
                width: 12
                height: 2
                color: mColor
            }
        }
    }
    Component
    {
        id:lineMeasureLineComponent
        MapPolyline
        {
            property color mColor: undefined
            id:lineRulerItem
            line.color: mColor
            line.width: 2
            smooth:true
            antialiasing: true
            function changeCoordinate(coord1, coord2){
                var path = lineRulerItem.path
                path[0] = coord1;
                path[1] = coord2;
                lineRulerItem.path  = path
            }
        }
    }
    Component
    {
        id:endMeasureLineComponent
        MapQuickItem
        {
            property color mColor: undefined
            anchorPoint.x: canvasArrow.width/2
            anchorPoint.y: 0

            sourceItem: Canvas
            {
                id: canvasArrow
                width:12
                height:8
                onPaint:
                {
                    var ctx = getContext("2d")
                    ctx.lineWidth = 1;
                    ctx.fillStyle = mColor
                    ctx.beginPath()
                    ctx.moveTo(width/2, 1)
                    ctx.lineTo(width,height)
                    ctx.lineTo(0, height)
                    ctx.fill()
                }
            }
        }
    }
    Component
    {
        id:textMeasureLineComponent
        MapQuickItem
        {
            property string text: "0.0 0m"
            property int widthRect: 80

            id:textQuickItem
            anchorPoint.x: rectText.width/2
            anchorPoint.y: 20
            sourceItem: Rectangle{
                height: 15
                width: textInfor.text.length * 7.5
                color: "transparent"
                Rectangle{
                    id: rectText
                    width: textInfor.text.length * 7.5
                    height: 15
                    color: "white"
                    radius: 4

                    Text {
                        id: textInfor
                        anchors.centerIn: parent
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment:  Text.AlignVCenter
                        color:"red"
                        font.family: "Arial"
                        font.pixelSize: 13
                        text: textQuickItem.text
                    }
                }
            }
        }
    }

    function updateData(data)
    {
        plane.coordinate = data["coordinate"];
        plane.rotation = data["yaw"];
        track.coordinate =  data["targetCoordinate"];
        var losPath = los.path;
        losPath[0] = data["coordinate"];
        losPath[1] = data["centerCoordinate"];
        los.path = losPath;

        trackComp.coordinate =  data["targetCoordinateComp"];
        var losCompPath = losComp.path;
        losCompPath[0] = data["coordinate"];
        losCompPath[1] = data["centerCoordinateComp"];
        losComp.path = losCompPath;

    }

    function centerMap()
    {
        root.center = plane.coordinate;
    }

    function centerTarget()
    {
        root.center = los.path[1];
    }

    function createMeasureline(coord1, coord2, color){
        if(color === undefined)
            color = measurelineColor
        var azimuth = coord1.azimuthTo(coord2)
        var distance = coord1.distanceTo(coord2)
        var coordCenter = coord1.atDistanceAndAzimuth(distance/2,azimuth)

        var measureLine = [];
        //create origin
        var startObject = startMeasureLineComponent.createObject(root,{"coordinate":coord1, mColor:color})
        if (startObject === undefined) {
            console.log("Error create origin");
            return;
        }else{
            startObject.transformOrigin = Item.Top
            startObject.rotation = azimuth
            root.addMapItem(startObject)
            measureLine[0] = startObject
        }
        //create line
        var lineObject = lineMeasureLineComponent.createObject(root,{mColor:color})
        if (lineObject === undefined) {
            console.log("Error create line");
            return;
        }else{
            lineObject.changeCoordinate(coord1,coord2)
            root.addMapItem(lineObject)
            measureLine[1] = lineObject
        }
        //create arrow
        var endObject = endMeasureLineComponent.createObject(root,{"coordinate":coord2, mColor:color})
        if (endObject === undefined) {
            console.log("Error create arrow");
            return;
        }else{
            endObject.transformOrigin = Item.Top
            endObject.rotation = azimuth
            root.addMapItem(endObject)
            measureLine[2] = endObject
        }
        //create text
        var textObject = textMeasureLineComponent.createObject(root,{"coordinate":coordCenter})
        var textRotation = 0
        if (textObject === undefined) {
            console.log("Error create text");
            return;
        }else{
            if(azimuth < 180)
                textRotation = azimuth - 90
            else
                textRotation = azimuth + 90
            textObject.transformOrigin = Item.Bottom
            textObject.rotation = textRotation
            textObject.text = qsTr(azimuth.toFixed(1) + "° " + distance.toFixed(0) + "m")

            root.addMapItem(textObject)
            measureLine[3] = textObject
        }

        if(measureLine){
            return measureLine;
        }
        else
            return undefined;
    }

    //change measureline coordinate
    function changeMeasureLine(measureLine, coord1, coord2){
        if(measureLine !== undefined){
            var azimuth = coord1.azimuthTo(coord2)
            var distance = coord1.distanceTo(coord2)
            var coordCenter = coord1.atDistanceAndAzimuth(distance/2,azimuth)
            var textRotation = 0
            if(azimuth < 180)
                textRotation = azimuth - 90
            else
                textRotation = azimuth + 90

            measureLine[0].coordinate = coord1
            measureLine[0].rotation = azimuth
            measureLine[1].changeCoordinate(coord1, coord2)
            measureLine[2].coordinate = coord2
            measureLine[2].rotation = azimuth
            measureLine[3].rotation = textRotation
            measureLine[3].text = qsTr(azimuth.toFixed(1) + "° " + distance.toFixed(0) + "m")
            measureLine[3].coordinate = coordCenter
        }
    }

    function removeLastMeasureLine(){
        if(listMeasureLine.length > 0){
            var lastMeasureLine = listMeasureLine.pop();
            if(lastMeasureLine !== undefined){
                for(var i = 0 ; i < 4 ; i ++){
                    lastMeasureLine[i].destroy()//delete all element
                }
            }
        }
    }

    function removeAllMeasureLines()
    {
        var numMeasureLine = listMeasureLine.length;
        for(var i=0;i<numMeasureLine;i++)
        {
            removeLastMeasureLine();
        }
    }
}
