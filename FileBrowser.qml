/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Mobility Components.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.1
import QtQuick.Controls 2.4
import Qt.labs.folderlistmodel 2.1

Rectangle {
    id: fileBrowser
    color: "black"
    property int iconSize: 30
    property string folder
    signal fileSelected(string filePath);
    Column {
        anchors.fill: parent
        Row {
            Button {
                width: iconSize * 2
                height: iconSize * 2
                text: "<"
                onClicked: {
                    if(folders.folder.toString() !== "file:///")
                    folders.folder = folders.parentFolder;

                }
            }
            Label {
                width: fileBrowser.width - iconSize
                height: iconSize * 2
                text: folders.folder
                color: "white"
                wrapMode: Label.WrapAnywhere
            }
        }
        Rectangle {
            width: fileBrowser.width
            height: 1
            color: "gray"
        }

        ListView {
            id: listView
            width: fileBrowser.width
            height: fileBrowser.height - iconSize * 2 - 1
            clip: true
            model: FolderListModel {
                id: folders
                showDotAndDotDot: false
                folder: "file:///home/evn/Videos"
                sortField: FolderListModel.Type
                nameFilters: ["*.mpeg4","*.ts","*.mp4","*.mkv","*.h264","*.h265"]
            }
            delegate: Rectangle {
                id: fileInfo
                height: iconSize
                width: parent.width
                color: listView.currentIndex === index ?
                           "lightgreen":"transparent"
                Row {
                    anchors.fill: parent
                    Image {
                        height: iconSize
                        fillMode: Image.PreserveAspectFit
                        source: "qrc:/GUI/assets/icon_Folder.png"
                        visible: folders.isFolder(index)
                    }
                    Label {
                        height: iconSize
                        width: parent.width - iconSize
                        text: fileName
                        horizontalAlignment: Label.AlignLeft
                        verticalAlignment: Label.AlignVCenter
                        color: "white"
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onPressed: {
                        listView.currentIndex = index;
                    }
                    onDoubleClicked: {
                        if(folders.isFolder(index))
                        {
                            if(folders.folder.toString() !== "file:///")
                                folders.folder = folders.folder + "/" + fileName;
                            else
                                folders.folder = folders.folder+fileName;
                        }
                        else {
                            fileSelected(filePath)
                        }
                    }
                }
            }

        }
    }
}
