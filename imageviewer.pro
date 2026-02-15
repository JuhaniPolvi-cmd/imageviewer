QT += widgets
qtHaveModule(printsupport): QT += printsupport

HEADERS       = imageviewer.h \
    Processor.h
SOURCES       = imageviewer.cpp \
                Processor.cpp \
                main.cpp

# install
target.path = $$[QT_INSTALL_EXAMPLES]/widgets/widgets/imageviewer
INSTALLS += target


wince {
   DEPLOYMENT_PLUGIN += qjpeg qgif
}
