/***************************************************************************
 *   Copyright (C) 2017 by Nicolas Carion                                  *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

import QtQuick 2.7
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.0
import QtQuick.Controls.Styles 1.4
import QtQuick.Window 2.2
import QtQml.Models 2.2

Rectangle {
    id: listRoot
    SystemPalette { id: activePalette }
    color: activePalette.window

    function expandNodes(indexes)  {
        for(var i = 0; i < indexes.length; i++) {
            if (indexes[i].valid) {
                listView.expand(indexes[i]);
            }
        }
    }
    function rowPosition(model, index) {
        var pos = 0;
        for(var i = 0; i < index.parent.row; i++) {
            var catIndex = model.getCategory(i);
            if (listView.isExpanded(catIndex)) {
                pos += model.rowCount(catIndex);
            }
            pos ++;
        }
        pos += index.row + 2;
        return pos;
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        ButtonGroup {
            id: radioGroup
        }

        RowLayout {
            id: row
            Layout.fillWidth: true
            Layout.fillHeight: false
            spacing: 4

            ToolButton {
                id: searchList
                icon.name: "edit-find"
                checkable: true
                ToolTip.visible: hovered
                ToolTip.text: isEffectList ? i18n("Find effect") : i18n("Find composition")
                onCheckedChanged: {
                    searchInput.visible = searchList.checked
                    searchInput.focus = searchList.checked
                    if (!searchList.checked) {
                        searchInput.text = ''
                        listView.focus = true
                    }
                }
            }
            ToolButton {
                id: showAll
                icon.name: "show-all-effects"
                checkable: true
                checked: true
                ButtonGroup.group: radioGroup
                ToolTip.visible: hovered
                ToolTip.text: isEffectList ? i18n("Main effects") : i18n("Main compositions")
                onClicked: {
                    assetlist.setFilterType("")
                }
            }
            ToolButton {
                id: showVideo
                visible: isEffectList
                icon.name: "kdenlive-show-video"
                icon.source: 'qrc:///pics/kdenlive-show-video.svgz'
                checkable:true
                ButtonGroup.group: radioGroup
                ToolTip.visible: hovered
                ToolTip.text: i18n("Show all video effects")
                onClicked: {
                    assetlist.setFilterType("video")
                }
            }
            ToolButton {
                id: showAudio
                visible: isEffectList
                icon.name: "kdenlive-show-audio"
                icon.source: 'qrc:///pics/kdenlive-show-audio.svgz'
                checkable:true
                ButtonGroup.group: radioGroup
                ToolTip.visible: hovered
                ToolTip.text: i18n("Show all audio effects")
                onClicked: {
                    assetlist.setFilterType("audio")
                }
            }
            ToolButton {
                id: showCustom
                visible: isEffectList
                icon.name: "kdenlive-custom-effect"
                checkable:true
                ButtonGroup.group: radioGroup
                ToolTip.visible: hovered
                ToolTip.text: i18n("Show all custom effects")
                onClicked: {
                    assetlist.setFilterType("custom")
                }
            }
            ToolButton {
                id: showFavorites
                icon.name: "favorite"
                checkable:true
                ButtonGroup.group: radioGroup
                ToolTip.visible: hovered
                ToolTip.text: i18n("Show favorite items")
                onClicked: {
                    assetlist.setFilterType("favorites")
                }
            }
            ToolButton {
                id: downloadTransitions
                visible: !isEffectList
                icon.name: "edit-download"
                ToolTip.visible: hovered
                ToolTip.text: i18n("Download New Wipes...")
                onClicked: {
                    assetlist.downloadNewLumas()
                }
            }
            Rectangle {
                //This is a spacer
                Layout.fillHeight: false
                Layout.fillWidth: true
                color: "transparent"
            }
            ToolButton {
                id: showDescription
                icon.name: "help-about"
                checkable:true
                ToolTip.visible: hovered
                ToolTip.text: isEffectList ? i18n("Show/hide description of the effects") : i18n("Show/hide description of the compositions")
                onCheckedChanged:{
                    assetlist.showDescription = checked
                }
                Component.onCompleted: checked = assetlist.showDescription
            }

        }
        TextField {
            id: searchInput
            Layout.fillWidth:true
            visible: false
            Image {
                id: clear
                source: 'image://icon/edit-clear'
                width: parent.height * 0.8
                height: width
                anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
                opacity: 0
                MouseArea {
                    anchors.fill: parent
                    onClicked: { searchInput.text = ''; searchInput.focus = true; searchList.checked = false; }
                }
            }
            states: State {
                name: "hasText"; when: searchInput.text != ''
                PropertyChanges { target: clear; opacity: 1 }
            }

            transitions: [
                Transition {
                    from: ""; to: "hasText"
                    NumberAnimation { properties: "opacity" }
                },
                Transition {
                    from: "hasText"; to: ""
                    NumberAnimation { properties: "opacity" }
                }
            ]
            onTextChanged: {
                var current = sel.currentIndex
                var rowModelIndex = assetListModel.getModelIndex(sel.currentIndex);
                assetlist.setFilterName(text)
                if (text.length > 0) {
                    sel.setCurrentIndex(assetListModel.firstVisibleItem(current), ItemSelectionModel.ClearAndSelect)
                } else {
                    sel.clearCurrentIndex()
                    sel.setCurrentIndex(assetListModel.getProxyIndex(rowModelIndex), ItemSelectionModel.ClearAndSelect)
                }
                listView.__listView.positionViewAtIndex(rowPosition(assetListModel, sel.currentIndex), ListView.Visible)
            }
            /*onEditingFinished: {
                if (!assetContextMenu.isDisplayed) {
                    searchList.checked = false
                }
            }*/
            Keys.onDownPressed: {
                sel.setCurrentIndex(assetListModel.getNextChild(sel.currentIndex), ItemSelectionModel.ClearAndSelect)
                listView.expand(sel.currentIndex.parent)
                listView.__listView.positionViewAtIndex(rowPosition(assetListModel, sel.currentIndex), ListView.Visible)
            }
            Keys.onUpPressed: {
                sel.setCurrentIndex(assetListModel.getPreviousChild(sel.currentIndex), ItemSelectionModel.ClearAndSelect)
                listView.expand(sel.currentIndex.parent)
                listView.__listView.positionViewAtIndex(rowPosition(assetListModel, sel.currentIndex), ListView.Visible)
            }
            Keys.onReturnPressed: {
                if (sel.hasSelection) {
                    assetlist.activate(sel.currentIndex)
                    searchList.checked = false
                }
            }
        }
        ItemSelectionModel {
            id: sel
            model: assetListModel
            onSelectionChanged: {
                assetDescription.text = i18n(assetlist.getDescription(sel.currentIndex))
            }
        }

        Column {
            Layout.fillHeight: true
            Layout.fillWidth: true

            ListView {
                id: listView

                Layout.fillHeight: true
                Layout.fillWidth: true

                delegate: Rectangle {
                    id: assetDelegate
                    // These anchors are important to allow "copy" dragging
                    anchors.verticalCenter: parent ? parent.verticalCenter : undefined
                    anchors.right: parent ? parent.right : undefined
                    property bool isItem : styleData.value !== "root" && styleData.value !== ""
                    property string mimeType : isItem ? assetlist.getMimeType(styleData.value) : ""
                    height: assetText.implicitHeight
                    color: dragArea.containsMouse ? activePalette.highlight : "transparent"

                    Drag.active: isItem ? dragArea.drag.active : false
                    Drag.dragType: Drag.Automatic
                    Drag.supportedActions: Qt.CopyAction
                    Drag.mimeData: isItem ? assetlist.getMimeData(styleData.value) : {}
                    Drag.keys:[
                        isItem ? assetlist.getMimeType(styleData.value) : ""
                    ]

                    Row {
                        anchors.fill:parent
                        anchors.leftMargin: 1
                        anchors.topMargin: 1
                        anchors.bottomMargin: 1
                        spacing: 4
                        Image{
                            id: assetThumb
                            anchors.verticalCenter: parent.verticalCenter
                            visible: assetDelegate.isItem
                            property bool isFavorite: model == undefined || model.favorite === undefined ? false : model.favorite
                            height: parent.height * 0.8
                            width: height
                            source: 'image://asseticon/' + styleData.value
                        }
                        Label {
                            id: assetText
                            font.bold : assetThumb.isFavorite
                            text: i18n(assetlist.getName(styleData.index))
                        }
                    }
                    MouseArea {
                        id: dragArea
                        anchors.fill: assetDelegate
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        drag.target: undefined
                        onReleased: {
                            drag.target = undefined
                        }
                        onPressed: {
                            if (assetDelegate.isItem) {
                                //sel.select(styleData.index, ItemSelectionModel.Select)
                                sel.setCurrentIndex(styleData.index, ItemSelectionModel.ClearAndSelect)
                                if (mouse.button === Qt.LeftButton) {
                                    drag.target = parent
                                    parent.grabToImage(function(result) {
                                        parent.Drag.imageSource = result.url
                                    })
                                } else {
                                    drag.target = undefined
                                    assetContextMenu.isItemFavorite = assetThumb.isFavorite
                                    assetContextMenu.popup()
                                    mouse.accepted = false
                                }
                                console.log(parent.Drag.keys)
                            } else {
                                if (listView.isExpanded(styleData.index)) {
                                    listView.collapse(styleData.index)
                                } else {
                                    listView.expand(styleData.index)
                                }

                            }
                            listView.focus = true
                        }
                        onDoubleClicked: {
                            if (isItem) {
                                assetlist.activate(styleData.index)
                            }
                        }
                    }
                }
                Menu {
                    id: assetContextMenu
                    property bool isItemFavorite
                    property bool isDisplayed: false
                    MenuItem {
                        id: favMenu
                        text: assetContextMenu.isItemFavorite ? i18n("Remove from favorites") : i18n("Add to favorites")
                        property url thumbSource
                        onTriggered: {
                            assetlist.setFavorite(sel.currentIndex, !assetContextMenu.isItemFavorite)
                        }
                    }
                    onAboutToShow: {
                        isDisplayed = true;
                    }
                    onAboutToHide: {
                        isDisplayed = false;
                    }
                }

                model: assetListModel

                Keys.onDownPressed: {
                    sel.setCurrentIndex(assetListModel.getNextChild(sel.currentIndex), ItemSelectionModel.ClearAndSelect)
                    listView.expand(sel.currentIndex.parent)
                    listView.__listView.positionViewAtIndex(rowPosition(assetListModel, sel.currentIndex), ListView.Visible)
                }
                Keys.onUpPressed: {
                    sel.setCurrentIndex(assetListModel.getPreviousChild(sel.currentIndex), ItemSelectionModel.ClearAndSelect)
                    listView.expand(sel.currentIndex.parent)
                    listView.__listView.positionViewAtIndex(rowPosition(assetListModel, sel.currentIndex), ListView.Visible)
                }
                Keys.onReturnPressed: {
                    if (sel.hasSelection) {
                        assetlist.activate(sel.currentIndex)
                    }
                }

            }

            TextArea {
                id: assetDescription
                width: parent.width
                height: Math.max(parent.height / 2, 50)
                text: ""
                visible: showDescription.checked
                readOnly: true

                states: State {
                    name: "hasDescription"; when: assetDescription.text != '' && showDescription.checked
                    PropertyChanges { target: assetDescription; visible: true}
                }
            }

        }
    }
}
