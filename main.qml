import QtQuick 2.9
import QtQuick.Controls 2.4
import QtQuick.Controls 1.4 as ControlOld
import QtQuick.Window 2.2
import Qt.labs.platform 1.0
import Qt.labs.folderlistmodel 2.1
import QtPositioning 5.0

import viettel.dev 1.0
Window {
    id: window
    visible: true
    width: 1280
    height: 720
    title: qsTr("GStreamerDecoder")
    VideoEngine {
        id: videoEngine
        onVideoInformationChanged : {
            lblInformation.text = information;
            itmPlayback.startTimer();
        }
        onMetadataFound: {
            map.updateData(data);
        }
    }

    Column {
        anchors.fill: parent
        ControlOld.SplitView {
            width: parent.width
            height: parent.height-itmPlayback.height
            Item {
                width: parent.width * 2 / 3
                height: parent.height
                VideoRender {
                    id: videoRender
                    anchors.fill: parent
                }
                Rectangle {
                    id: rectRender
                    x: videoRender.drawPosition.x
                    y: videoRender.drawPosition.y
                    width: videoRender.drawPosition.width
                    height: videoRender.drawPosition.height
                    border.color: "red"
                    border.width: 1
                    color: "transparent"

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        color: "red"
                        width: parent.width
                        height: 1
                    }
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        color: "red"
                        width: 1
                        height: parent.height
                    }
                    Repeater{
                       anchors.fill: parent
                       model: videoEngine.geoPoints
                       delegate: Rectangle {
                           width: 10
                           height: width
                           color: "orange"
                           x: point.x / size.width * rectRender.width - width / 2
                           y: point.y / size.height * rectRender.height - height / 2
                           radius: width/2
                           opacity: 0.5
                       }
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            videoEngine.computeTargetLocation(mouseX/width,mouseY/height)
                        }
                    }
                }



            }
            Map {
                id: map
                height: parent.height
                listClickPoint : videoEngine.geoPoints
                Column {
                    spacing: 5
                    Row {
                        spacing: 5
                        Button {
                            text: "UAV"
                            onClicked: {
                                map.centerMap();
                            }
                        }
                        Button {
                            text: "TGT"
                            onClicked: {
                                map.centerTarget();
                            }
                        }
                        Button {
                            text: "Delete ruler"
                            onClicked: {
                                map.removeLastMeasureLine();
                            }
                        }
                        Button {
                            text: "Clear rulers"
                            onClicked: {
                                map.removeAllMeasureLines();
                            }
                        }
                    }
                    Row {
                        spacing: 5
                        Label {
                            text: "Sx"
                            color: "white"
                        }

                        SpinBox {
                            id: txtSx
                            value: 33
                            stepSize: 1
                            onValueChanged: {
                                videoEngine.setSensorParams(txtSx.value,txtSy.value);
                            }
                        }
                    }
                    Row {
                        spacing: 5
                        Label {
                            text: "Sy"
                            color: "white"
                        }

                        SpinBox {
                            id: txtSy
                            value: 25
                            stepSize: 1
                            onValueChanged: {
                                videoEngine.setSensorParams(txtSx.value,txtSy.value);
                            }
                        }
                    }
                }
            }
        }

        Playback {
            id: itmPlayback
            width: parent.width
            height: 62
        }
    }
    Column {
        y: 10
        Button {
            width: 30
            height: 30
            opacity: 0.5
            text: clmVideo.visible?"<":">"
            onClicked: {
                clmVideo.visible =! clmVideo.visible;
            }
        }
    }

    Column {
        id: clmVideo
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin:  40
        anchors.topMargin: 10
        visible: false
        Row {
            spacing: 10
            ComboBox {
                id: cbxDecoderType
                model: videoEngine.decoderList
            }
            Button {
                id: btnChangeDecoder
                text: "Change Decoder"
                onClicked: videoEngine.changeDecoder(cbxDecoderType.currentText)
            }
        }
        Label
        {
            id: lblInformation
            color: "white"
        }
        FileBrowser {
            width: 300
            height: 500
            onFileSelected: {
                videoEngine.setVideo(filePath);
            }
        }
    }

    Component.onCompleted: {
        videoEngine.setRender(videoRender);
//        videoEngine.setVideo("/home/hainh/Desktop/Video/Log/VT-Video 25-12-2021 11-06-59.ts");
        videoEngine.setVideo("/home/hainh/Desktop/MISB/Samples/Locks_to_St.ts");
    }
}
