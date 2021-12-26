import QtQuick 2.0
import QtQuick.Controls 2.4
Rectangle {
    id: root
    width: 600
    height: 62
    color: "#1a1a1a"
    focus: true

    signal selectVideoClicked()

    function startTimer() {
        timer.start();
        lblTimeTotal.text = "00:00";
        lblTimeCurrent.text = "00:00";
    }

    function stopTimer() {
        timer.stop();
    }

    Button {
        id: btnPause
        y: 11
        width: 74
        height: 40
        text: pause === false?qsTr("Pause"):qsTr("Continue")
        anchors.leftMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        property bool pause: false
        onClicked: {
            pause = !pause;
            videoEngine.pause(pause);
        }
    }



    Button {
        id: btnNext
        y: 11
        width: 74
        height: 40
        text: qsTr("Next")
        anchors.left: btnPause.right
        anchors.leftMargin: 6
        anchors.verticalCenter: parent.verticalCenter
    }

    Slider {
        id: sldTime
        y: 11
        height: 40
        anchors.right: lblTimeTotal.left
        anchors.rightMargin: 12
        anchors.left: lblTimeCurrent.right
        anchors.leftMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        value: 0
        onPressedChanged: {
            if(!pressed) videoEngine.goToPosition(visualPosition);
        }
    }

    Label {
        id: lblTimeCurrent
        y: 11
        width: 84
        height: 40
        color: "#ffffff"
        text: qsTr("00:00")
        anchors.left: cbxSpeed.right
        anchors.leftMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
    }

    Label {
        id: lblTimeTotal
        x: 1188
        y: 11
        width: 84
        height: 40
        color: "#ffffff"
        text: qsTr("00:00")
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    ComboBox {
        id: cbxSpeed
        y: 11
        width: 74
        height: 40
        anchors.left: btnNext.right
        anchors.leftMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        down: false
        model: ["0.03x","0.25x","0.5x","1x","2x","4x","8x"]
        currentIndex: 3
        onCurrentTextChanged: {
            var speed = 1;
            switch(currentText){
            case "0.03x": speed = 0.03; break;
            case "0.25x": speed = 0.25; break;
            case "0.5x": speed = 0.5; break;
            case "1x": speed = 1; break;
            case "2x": speed = 2; break;
            case "4x": speed = 4; break;
            case "8x": speed = 8; break;
            }
            videoEngine.setSpeed(speed);
        }
    }
    Timer{
        id: timer
        interval: 1000
        repeat: true
        running: false
        onTriggered: {
            var totalTime = videoEngine.getDuration()/1000;
            var posCurrent = videoEngine.getCurrentTime()/1000;
            var value = posCurrent/totalTime;
            var totalTimeSeconds = Number(totalTime % 60).toFixed(0);
            var totalTimeMinute = Number((totalTime - (totalTime % 60)) / 60).toFixed(0);
            lblTimeTotal.text = totalTimeMinute.toString()+":"+totalTimeSeconds.toString();
            var posCurrentSeconds = Number(posCurrent % 60).toFixed(0);
            var posCurrentMinute = Number((posCurrent - (posCurrent % 60)) / 60).toFixed(0);
            lblTimeCurrent.text = posCurrentMinute.toString()+":"+posCurrentSeconds.toString();
            if(!sldTime.pressed)sldTime.value = value;
        }
    }
}
