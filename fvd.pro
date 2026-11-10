#-------------------------------------------------
#
#    FVD++, an advanced coaster design tool for NoLimits
#    Copyright (C) 2012-2015, Stephan "Lenny" Alt <alt.stephan@web.de>
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program. If not, see <http://www.gnu.org/licenses/>.
#
#-------------------------------------------------


# DEPENDENCIES
# 
# QT (tested with 5.4.0)
# glew (for Windows and Linux builds only)
# glm (tested with 0.9.5.1-1)
# lib3ds

CONFIG	+= qt
QT       += core gui widgets printsupport opengl

#CONFIG += exceptions \
#          rtti

# glm 0.9.9 and later put the gtx headers the code uses (vector_angle,
# rotate_vector, norm) behind this switch; older glm ignores it.
DEFINES += GLM_ENABLE_EXPERIMENTAL

# glm 0.9.9 and later leave default-constructed vectors and matrices
# uninitialised; the glm 0.9.5 this code was written for zeroed vectors and
# made matrices the identity. This restores that; older glm ignores it.
DEFINES += GLM_FORCE_CTOR_INIT

TARGET = FVD
TEMPLATE = app

SOURCES += main.cpp\
    core/undohandler.cpp \
    core/undoaction.cpp \
    core/trackhandler.cpp \
    core/track.cpp \
    core/subfunction.cpp \
    core/smoothhandler.cpp \
    core/sectionhandler.cpp \
    core/section.cpp \
    core/secstraight.cpp \
    core/secgeometric.cpp \
    core/secforced.cpp \
    core/seccurved.cpp \
    core/secbezier.cpp \
    core/saver.cpp \
    core/nolimitsimporter.cpp \
    core/mnode.cpp \
    core/function.cpp \
    core/exportfuncs.cpp \
    osx/common.cpp \
    renderer/trackmesh.cpp \
    renderer/mytexture.cpp \
    renderer/myshader.cpp \
    renderer/myframebuffer.cpp \
    renderer/glviewwidget.cpp \
    ui/transitionwidget.cpp \
    ui/trackwidget.cpp \
    ui/trackproperties.cpp \
    ui/smoothui.cpp \
    ui/qcustomplot.cpp \
    ui/projectwidget.cpp \
    ui/optionsmenu.cpp \
    ui/objectexporter.cpp \
    ui/mytreewidget.cpp \
    ui/myqdoublespinbox.cpp \
    ui/mainwindow.cpp \
    ui/importui.cpp \
    ui/graphwidget.cpp \
    ui/graphhandler.cpp \
    ui/exportui.cpp \
    ui/draglabel.cpp \
    ui/conversionpanel.cpp \
    core/secnlcsv.cpp

HEADERS  += core/undohandler.h \
    core/undoaction.h \
    core/trackhandler.h \
    core/track.h \
    core/subfunction.h \
    core/smoothhandler.h \
    core/sectionhandler.h \
    core/section.h \
    core/secstraight.h \
    core/secgeometric.h \
    core/secforced.h \
    core/seccurved.h \
    core/secbezier.h \
    core/saver.h \
    core/nolimitsimporter.h \
    core/mnode.h \
    core/function.h \
    core/exportfuncs.h \
    osx/common.h \
    renderer/trackmesh.h \
    renderer/mytexture.h \
    renderer/myshader.h \
    renderer/myframebuffer.h \
    renderer/glviewwidget.h \
    ui/transitionwidget.h \
    ui/trackwidget.h \
    ui/trackproperties.h \
    ui/smoothui.h \
    ui/qcustomplot.h \
    ui/projectwidget.h \
    ui/optionsmenu.h \
    ui/objectexporter.h \
    ui/mytreewidget.h \
    ui/myqdoublespinbox.h \
    ui/mainwindow.h \
    ui/importui.h \
    ui/graphwidget.h \
    ui/graphhandler.h \
    ui/exportui.h \
    ui/draglabel.h \
    ui/conversionpanel.h \
    lenassert.h \
    core/secnlcsv.h

SOURCES += core/traintype.cpp
HEADERS += core/traintype.h

FORMS    += ui/transitionwidget.ui \
    ui/trackwidget.ui \
    ui/trackproperties.ui \
    ui/smoothui.ui \
    ui/projectwidget.ui \
    ui/optionsmenu.ui \
    ui/objectexporter.ui \
    ui/mainwindow.ui \
    ui/importui.ui \
    ui/graphwidget.ui \
    ui/exportui.ui \
    ui/conversionpanel.ui

!unix:!macx {
    INCLUDEPATH += "./ui/"
    INCLUDEPATH += "./renderer/"
    INCLUDEPATH += "./core/"

    RC_FILE = winicon.rc

    # Where GLEW, glm and lib3ds come from is a property of the machine, not of
    # the project. If a libs.pri sits next to this file it is used (see
    # libs.pri.example); otherwise the libraries are expected on the compiler's
    # default search path, which is how the MSYS2 MinGW packages install them
    # (docs/building.md).
    exists($$PWD/libs.pri) {
        include($$PWD/libs.pri)
    } else {
        LIBS += -lglew32
        with3ds: LIBS += -l3ds
    }

    # The experimental .3ds model export needs lib3ds, which MSYS2 does not
    # package. It is built in only when asked for: qmake CONFIG+=with3ds
    with3ds: DEFINES += FVD_WITH_3DS

    LIBS += -lopengl32
    LIBS += -lglu32
}

unix:!macx {
    INCLUDEPATH += "./ui/"
    INCLUDEPATH += "./renderer/"
    INCLUDEPATH += "./core/"

    LIBS += -lGL
    LIBS += -lGLU
    LIBS += -lGLEW

    LIBS += -lX11
    LIBS += -L /usr/local/lib/
    LIBS += -l3ds
    DEFINES += FVD_WITH_3DS
}

macx {
    ICON = fvd.icns
    QMAKE_INFO_PLIST = ./osx/resources/Info.plist

    INCLUDEPATH += "./ui/"
    INCLUDEPATH += "./renderer/"
    INCLUDEPATH += "./core/"
    INCLUDEPATH += "./glm/"
    INCLUDEPATH += "/usr/local/include/"

    LIBS += -framework Foundation -framework Cocoa
    LIBS += -L /usr/local/lib/
    LIBS += -l3ds
    DEFINES += FVD_WITH_3DS

    QMAKE_CFLAGS_X86_64 += -mmacosx-version-min=10.6
    QMAKE_CXXFLAGS_X86_64 = $$QMAKE_CFLAGS_X86_64
    QMAKE_CXXFLAGS_RELEASE += -O2

    OBJECTIVE_SOURCES += \
        osx/Document.mm \
        osx/MainDelagate.mm \
        osx/NSApplicationMain.mm

    HEADERS += \
        osx/Document.h \
        osx/MainDelagate.h \
        osx/NSApplicationMain.h

    SOURCES +=  \
        osx/Init.cpp
}

RESOURCES += \
    resources.qrc

OTHER_FILES += \
    winicon.rc \
    background.png \
    metal.png \
    shaders/normals.vert \
    shaders/track.vert \
    shaders/track.frag \
    shaders/sky.vert \
    shaders/sky.frag \
    shaders/simpleSM.vert \
    shaders/simpleSM.frag \
    shaders/shadowVolume.vert \
    shaders/shadowVolume.frag \
    shaders/oculus.vert \
    shaders/oculus.frag \
    shaders/occlusion.vert \
    shaders/occlusion.frag \
    shaders/normals.frag \
    shaders/metal.dat \
    shaders/floor.vert \
    shaders/floor.frag \
    shaders/debug.vert \
    shaders/debug.frag \
    metalnormals.png \
    readme.txt \
    sky/negx.jpg \
    sky/negy.jpg \
    sky/negz.jpg \
    sky/posx.jpg \
    sky/posy.jpg \
    sky/posz.jpg

# Batch mode for testing (docs/testing.md)
SOURCES += ui/batchrunner.cpp ui/batchscenarios.cpp
HEADERS += ui/batchrunner.h ui/batchscenarios.h

# Core separation (docs/decisions/0007)
HEADERS += core/tracklistener.h
