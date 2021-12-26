import QtQuick 2.9
import QtQuick.Controls 2.4
import QtQuick.Controls 1.4 as ControlOld
import QtQuick.Window 2.2
import Qt.labs.platform 1.0
import Qt.labs.folderlistmodel 2.1

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
            VideoRender {
                id: videoRender
                width: parent.width * 2 / 3
                height: parent.height
            }
            Map {
                id: map
                height: parent.height
                Button {
                    text: "UAV"
                    onClicked: {
                        map.centerMap();
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
                model: ["FFMPEG","GSTREAMER"]
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
    }
}
