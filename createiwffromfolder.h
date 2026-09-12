//*********************************************************************************************************
//createiwffromfolder.h
//*********************************************************************************************************
#ifndef CREATEIWFFROMFOLDER_H
#define CREATEIWFFROMFOLDER_H

#include <QDir>
#include <QString>

struct ImageInfo {
    QString fileName;
    QString baseName;
    QString parentName;
    QString absPath;
    int width = 0;
    int height = 0;
    bool hasAlpha = false;
};

struct EntryToPack {
    QString logicalName;
    QString absPath;
    bool isImage = false;
    bool forceOpaquePreview = false;
};

struct AssetCandidate {
    QString logicalName;
    QByteArray data;
};

bool createIwfFromFolder(const QDir &srcDir, const QString &outPath);

#endif // CREATEIWFFROMFOLDER_H
