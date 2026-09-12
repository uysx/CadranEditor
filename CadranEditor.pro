QT += core gui widgets

TEMPLATE = app
TARGET = CadranEditor

SOURCES += \
    create_sql.cpp \
    createiwffromfolder.cpp \
    dialogeditpicture.cpp \
    editdialog.cpp \
    iwflz_compress.cpp \
    iwflzcompress.cpp \
    jsonwatchface.cpp \
    main.cpp \
    mainwindow.cpp \
    pngtoveryfitraw.cpp \
    veryfitlzprofessor.cpp \
    utils.cpp

HEADERS += \
    create_sql.h \
    createiwffromfolder.h \
    dialogeditpicture.h \
    editdialog.h \
    iwflz_compress.h \
    iwflzcompress.h \
    jsonwatchface.h \
    mainwindow.h \
    pngtoveryfitraw.h \
    veryfitlzprofessor.h \
    utils.h

FORMS += \
    dialogeditpicture.ui \
    editdialog.ui \
    mainwindow.ui
