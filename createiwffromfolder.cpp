//*********************************************************************************************************
// createiwffromfolder.cpp - VERSION MODIFIÉE
// Changement : Parse iwf.json pour extraire "bkground" et "preview"
//              au lieu d'utiliser les heuristiques de taille
//*********************************************************************************************************

#include "createiwffromfolder.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QVector>
#include <QByteArray>
#include <QDataStream>
#include <QtEndian>
#include <QDirIterator>
#include <QMap>
#include <QSet>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <cstring> // memset, memcpy
#include <algorithm>

#include "pngtoveryfitraw.h" // PngToVeryfitRaw::probeImage / convertAuto


// ==========================================================================
// 1. SCAN DE TOUTES LES IMAGES
// ==========================================================================

static QVector<ImageInfo> scanAllImages(const QDir &srcDir)
{
    QVector<ImageInfo> out;

    QDirIterator it(
        srcDir.absolutePath(),
        QStringList() << "*.png" << "*.bmp",
        QDir::Files,
        QDirIterator::Subdirectories
        );

    while (it.hasNext()) {
        QString abs = it.next();
        QFileInfo fi(abs);

        int w = 0, h = 0;
        bool hasAlpha = false;
        if (!PngToVeryfitRaw::probeImage(abs, w, h, hasAlpha)) {
            continue;
        }

        ImageInfo info;
        info.fileName   = fi.fileName();              // "2.png"
        info.baseName   = fi.completeBaseName();      // "2"
        info.parentName = fi.dir().dirName();         // "g13", "week", ...
        info.absPath    = abs;
        info.width      = w;
        info.height     = h;
        info.hasAlpha   = hasAlpha;

        out.push_back(info);
    }

    return out;
}

// trouver première image qui matche un prédicat
static int findFirstImageIndex(const QVector<ImageInfo> &all,
                               std::function<bool(const ImageInfo&)> pred)
{
    for (int i = 0; i < all.size(); ++i) {
        if (pred(all[i])) return i;
    }
    return -1;
}

// recherche par prefix (utilisé pour week_en_xxx)
static int findByPrefix(const QVector<ImageInfo> &all, const QString &prefixLower)
{
    for (int i = 0; i < all.size(); ++i) {
        if (all[i].fileName.toLower().startsWith(prefixLower))
            return i;
    }
    return -1;
}


// ==========================================================================
// 1B. NOUVEAU : PARSER iwf.json POUR EXTRAIRE bkground ET preview
// ==========================================================================

struct IwfJsonInfo {
    QString backgroundFile;  // valeur de "bkground"
    QString previewFile;     // valeur de "preview"
    bool valid = false;
};

static IwfJsonInfo parseIwfJson(const QDir &srcDir)
{
    IwfJsonInfo info;

    QString iwfPath = srcDir.absoluteFilePath("iwf.json");
    QFile file(iwfPath);

    if (!file.open(QIODevice::ReadOnly)) {
        // iwf.json n'existe pas ou n'est pas lisible
        return info;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        // Erreur de parsing JSON
        return info;
    }

    if (!doc.isObject()) {
        return info;
    }

    QJsonObject root = doc.object();

    // Extraire "bkground"
    if (root.contains("bkground") && root["bkground"].isString()) {
        info.backgroundFile = root["bkground"].toString();
    }

    // Extraire "preview"
    if (root.contains("preview") && root["preview"].isString()) {
        info.previewFile = root["preview"].toString();
    }

    // Considéré valide si au moins un des deux champs existe
    info.valid = !info.backgroundFile.isEmpty() || !info.previewFile.isEmpty();

    return info;
}


// ==========================================================================
// 2. LECTURE DE font.json POUR L'ORDRE DES BANQUES
// ==========================================================================
//
// font.json :
// {
//   "item": [
//     { "name": "week", "bpp":16,"format":"png" },
//     { "name": "g13",  ... },
//     { "name": "g41",  ... },
//     { "name": "animal", ... }
//     ...
//   ]
// }
//
// On extrait uniquement l'ordre des "name" pour savoir dans quel ordre
// empaqueter les banques.
//

static QStringList readFontOrder(const QDir &srcDir)
{
    QStringList out;

    QString fontJsonPath = srcDir.absoluteFilePath("font.json");
    QFile file(fontJsonPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return out;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return out;

    if (!doc.isObject())
        return out;

    QJsonObject root = doc.object();
    if (!root.contains("item") || !root["item"].isArray())
        return out;

    QJsonArray arr = root["item"].toArray();
    for (const QJsonValue &val : arr) {
        if (!val.isObject())
            continue;

        QJsonObject obj = val.toObject();
        if (!obj.contains("name") || !obj["name"].isString())
            continue;

        QString n = obj["name"].toString().trimmed();
        if (n.isEmpty())
            continue;

        out.append(n);
    }

    return out;
}


// ==========================================================================
// 3. CONSTRUCTION DES BANQUES (glyphes indexés)
// ==========================================================================
//
// Ex: si on a un dossier "g13" avec 2.png, 10.png, 11.png
//     on crée BankInfo{ "g13", { 2 -> imgIdx, 10 -> imgIdx, 11 -> imgIdx } }
//
// On tente de parser le baseName de l'image comme un nombre entier => glyphId
//

struct BankInfo {
    QString bankName;              // "g13", "animal", "num_blanc_h_55", ...
    QMap<int, int> glyphs;         // glyphId -> imageIndex dans allImages
};

static bool tryParseNumeric(const QString &str, int &outVal)
{
    bool ok = false;
    outVal = str.toInt(&ok);
    return ok;
}

static QMap<QString, BankInfo> buildBanks(const QVector<ImageInfo> &allImages, const QDir &srcDir)
{
    QMap<QString, BankInfo> banks;

    QString rootName = srcDir.dirName();

    for (int i = 0; i < allImages.size(); ++i) {
        const auto &im = allImages[i];

        // ignorer images à la racine (parentName vide ou == rootName)
        if (im.parentName.isEmpty() || im.parentName == rootName || im.parentName == ".") {
            continue;
        }

        // essayer d'interpréter baseName comme un entier (0,1,2,10,28,...)
        int glyphId = -1;
        if (!tryParseNumeric(im.baseName, glyphId)) {
            // si ce n'est pas un entier clair, on l'ignore pour l'instant
            continue;
        }

        // enregistrer
        BankInfo &bank = banks[im.parentName];
        bank.glyphs[glyphId] = i;
    }

    return banks;
}


// ==========================================================================
// 4. ORDRE MAÎTRE GLOBAL DES INDEX
// ==========================================================================
//
// Ordre "universel" qu'on a déduit à partir des .iwf officiels.
// Quand une banque ne contient que 10 images, on filtre simplement
// les valeurs qui n'existent pas dans cette banque.
//
static const int GENERAL_ORDER[] = {
    26,24,2,11,37,6,38,15,21,28,36,29,16,3,10,40,
    4,33,39,35,18,34,22,31,27,9,7,1,19,25,32,20,
    14,0,23,12,30,5,17,13,8
};
static const int GENERAL_ORDER_LEN =
    sizeof(GENERAL_ORDER)/sizeof(GENERAL_ORDER[0]);


// ==========================================================================
// 5. OUTILS POUR EMPAQUETER DES ENTRÉES
// ==========================================================================

static void addIfExistsFile(QVector<EntryToPack> &result,
                            const QString &logicalName,
                            const QString &absPath)
{
    QFileInfo fi(absPath);
    if (!fi.exists())
        return;

    EntryToPack e;
    e.logicalName = logicalName;
    e.absPath     = fi.absoluteFilePath();
    e.isImage     = false;
    e.forceOpaquePreview = false;
    result.push_back(e);
}

static void addImageByAllIndex(QVector<EntryToPack>      &result,
                               const QVector<ImageInfo>  &allImages,
                               int                        allIdx,
                               bool                       forceOpaquePrev,
                               const QString             &overrideLogicalName = QString())
{
    if (allIdx < 0 || allIdx >= allImages.size())
        return;

    const auto &img = allImages[allIdx];

    EntryToPack e;
    if (!overrideLogicalName.isEmpty()) {
        // nom forcé (ex "g13_2", "animal_28", "week_en_mon")
        e.logicalName = overrideLogicalName;
    } else {
        // fallback : pour background/preview/racine
        if (!img.parentName.isEmpty()
            && img.parentName != "."
            && img.parentName != QFileInfo(img.absPath).dir().dirName()) {
            // note: ce fallback ne sera normalement pas utilisé pour les digits,
            // car on passe overrideLogicalName
            e.logicalName = img.parentName + "_" + img.baseName;
        } else {
            // image à la racine -> garder nom fichier
            e.logicalName = img.fileName;
        }
    }

    e.absPath = img.absPath;
    e.isImage = true;
    e.forceOpaquePreview = forceOpaquePrev;
    result.push_back(e);
}


// ==========================================================================
// 6. ÉMISSION D'UNE BANQUE NORMALE (g13, g41, animal, num_blanc_h_55, etc.)
// ==========================================================================
//
// On parcourt GENERAL_ORDER, et pour chaque valeur N, si la banque a N,
// on l'ajoute à result en la nommant "<bankName>_<N>" dans le .iwf.
//

static void emitBankInOrder(const QString              &bankName,
                            const BankInfo             &bank,
                            const QVector<ImageInfo>   &allImages,
                            QVector<EntryToPack>       &result)
{
    for (int oi = 0; oi < GENERAL_ORDER_LEN; ++oi) {
        int glyphId = GENERAL_ORDER[oi];
        if (!bank.glyphs.contains(glyphId))
            continue;

        int imgIdx = bank.glyphs[glyphId];

        QString logical = QString("%1_%2").arg(bankName).arg(glyphId);

        addImageByAllIndex(result,
                           allImages,
                           imgIdx,
                           /*forceOpaquePrev=*/false,
                           /*overrideLogicalName=*/logical);
    }
}


// ==========================================================================
// 7. ÉMISSION DE LA BANQUE "week"
// ==========================================================================
//
// On veut un ordre des jours cohérent. D'après les dumps VeryFit,
// l'ordre observé final dans l'IWF était :
//   week_en_tue, week_en_fri, week_en_sat, week_en_sun,
//   week_en_thur, week_en_wed, week_en_mon
//
// Sauf que pour l'UI c'est plus naturel mon,tue,wed,thur,fri,sat,sun.
// On va coller STRICTEMENT à ce que tu as observé dans les hexdumps,
// parce que c'est ce que VeryFit attend dans l'ordre binaire.
//
// Tu peux changer l'ordre si besoin selon les dumps que tu considères
// comme "référence".
//
static const char *WEEK_ORDER_OBSERVED[] = {
    "tue", "fri", "sat", "sun", "thur", "wed", "mon"
};
static const int WEEK_ORDER_OBSERVED_LEN = 7;

static void emitWeekBank(const QVector<ImageInfo> &allImages,
                         QVector<EntryToPack>     &result)
{
    for (int di = 0; di < WEEK_ORDER_OBSERVED_LEN; ++di) {
        QString suf = WEEK_ORDER_OBSERVED[di]; // "tue"
        QString wantFile = QString("en_%1.png").arg(suf).toLower(); // "en_tue.png"
        QString logical  = QString("week_en_%1").arg(suf);          // "week_en_tue"

        int foundIdx = -1;
        for (int i = 0; i < allImages.size(); ++i) {
            const auto &img = allImages[i];

            if (img.parentName.compare("week", Qt::CaseInsensitive) != 0)
                continue;

            if (img.fileName.toLower() == wantFile) {
                foundIdx = i;
                break;
            }
        }

        if (foundIdx >= 0) {
            addImageByAllIndex(result,
                               allImages,
                               foundIdx,
                               /*forceOpaquePrev=*/false,
                               /*overrideLogicalName=*/logical);
        }
    }
}

// ==========================================================================
// 7.1 ÉMISSION DE LA BANQUE "week"
// ==========================================================================
//
// On veut un ordre des jours cohérent. D'après les dumps VeryFit,
// l'ordre observé final dans l'IWF était :
//   week_en_tue, week_en_fri, week_en_sat, week_en_sun,
//   week_en_thur, week_en_wed, week_en_mon
//
// Sauf que pour l'UI c'est plus naturel mon,tue,wed,thur,fri,sat,sun.
// On va coller STRICTEMENT à ce que tu as observé dans les hexdumps,
// parce que c'est ce que VeryFit attend dans l'ordre binaire.
//
// Tu peux changer l'ordre si besoin selon les dumps que tu considères
// comme "référence".
//
static const char *MOUNTH_ORDER_OBSERVED[] = {
    "nov", "oct", "dec", "may", "june", "apr", "jan", "feb", "sept", "july", "mar", "aug"

};
static const int MONTH_ORDER_OBSERVED_LEN = 12;

static void emitMonthBank(const QVector<ImageInfo> &allImages,
                         QVector<EntryToPack>     &result)
{
    for (int di = 0; di < MONTH_ORDER_OBSERVED_LEN; ++di) {
        QString suf = MOUNTH_ORDER_OBSERVED[di]; // "mon"
        QString wantFile = QString("en_%1.png").arg(suf).toLower(); // "en_mon.png"
        QString logical  = QString("month_en_%1").arg(suf);          // "month_en_tue"

        int foundIdx = -1;
        for (int i = 0; i < allImages.size(); ++i) {
            const auto &img = allImages[i];

            if (img.parentName.compare("month", Qt::CaseInsensitive) != 0)
                continue;

            if (img.fileName.toLower() == wantFile) {
                foundIdx = i;
                break;
            }
        }

        if (foundIdx >= 0) {
            addImageByAllIndex(result,
                               allImages,
                               foundIdx,
                               /*forceOpaquePrev=*/false,
                               /*overrideLogicalName=*/logical);
        }
    }
}


// ==========================================================================
// 8. CONSTRUIRE LA LISTE FINALE DES ENTRÉES DANS L'ORDRE IWF
// ==========================================================================
//
// NOUVEL ORDRE :
//
//   1. iwf.json
//   2. font.json (si existe)
//   3. background (nom défini dans iwf.json "bkground")
//   4. preview (nom défini dans iwf.json "preview", avec forceOpaquePreview=true)
//   5. toutes les autres images du 1er niveau (racine) qui ne sont ni bg ni preview
//   6. pour chaque "name" dans font.json.item[] dans l'ordre :
//         - si name == "week": emitWeekBank()
//         - sinon si on a une banque avec ce nom: emitBankInOrder()
//


static QVector<EntryToPack> buildOrderedEntryList(const QDir &srcDir)
{
    QVector<EntryToPack> result;

    // (A) images dispo
    QVector<ImageInfo> allImages = scanAllImages(srcDir);

    // (B) parser iwf.json pour obtenir les noms de background et preview
    IwfJsonInfo iwfInfo = parseIwfJson(srcDir);

    // (C) l'ordre des banques dans font.json
    QStringList fontOrder = readFontOrder(srcDir);

    // ========== 1. iwf.json + font.json au début ==========
    addIfExistsFile(result, "iwf.json",  srcDir.absoluteFilePath("iwf.json"));
    addIfExistsFile(result, "font.json", srcDir.absoluteFilePath("font.json"));

    // ========== 3 & 4 & 5. Parser iwf.json pour extraire TOUTES les images référencées ==========

    // Ensemble pour éviter les doublons
    QSet<QString> referencedImages;

    // Parser iwf.json
    QString iwfPath = srcDir.absoluteFilePath("iwf.json");
    QFile iwfFile(iwfPath);
    if (iwfFile.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(iwfFile.readAll());
        iwfFile.close();

        if (doc.isObject()) {
            QJsonObject root = doc.object();

            // Clés à chercher au niveau racine
            QStringList rootKeys = {"bkground", "preview"};
            for (const QString& key : rootKeys) {
                if (root.contains(key) && root[key].isString()) {
                    QString fileName = root[key].toString();
                    if (!fileName.isEmpty()) {
                        referencedImages.insert(fileName);
                    }
                }
            }

            // Parcourir les items pour trouver les images
            if (root.contains("item") && root["item"].isArray()) {
                QJsonArray items = root["item"].toArray();

                // Clés d'images possibles dans les items
                QStringList itemKeys = {"hour", "minute", "second", "bg", "animaicon", "progress"};

                for (const QJsonValue& val : items) {
                    if (!val.isObject()) continue;

                    QJsonObject item = val.toObject();

                    for (const QString& key : itemKeys) {
                        if (item.contains(key) && item[key].isString()) {
                            QString fileName = item[key].toString();
                            if (!fileName.isEmpty()) {
                                referencedImages.insert(fileName);
                            }
                        }
                    }
                }
            }
        }
    }

    // ========== Ajouter les images référencées dans l'ordre : background, preview, puis les autres ==========

    // 3. Background en premier
    if (!iwfInfo.backgroundFile.isEmpty() && referencedImages.contains(iwfInfo.backgroundFile)) {
        int idxBg = findFirstImageIndex(allImages, [&](const ImageInfo &im) {
            return im.fileName.compare(iwfInfo.backgroundFile, Qt::CaseInsensitive) == 0;
        });

        if (idxBg >= 0) {
            addImageByAllIndex(result, allImages, idxBg, /*forceOpaquePrev=*/false);
            referencedImages.remove(iwfInfo.backgroundFile);  // Déjà traité
        }
    }

    // 4. Preview ensuite
    if (!iwfInfo.previewFile.isEmpty() && referencedImages.contains(iwfInfo.previewFile)) {
        int idxPrev = findFirstImageIndex(allImages, [&](const ImageInfo &im) {
            return im.fileName.compare(iwfInfo.previewFile, Qt::CaseInsensitive) == 0;
        });

        if (idxPrev >= 0) {
            addImageByAllIndex(result, allImages, idxPrev, /*forceOpaquePrev=*/true);
            referencedImages.remove(iwfInfo.previewFile);  // Déjà traité
        }
    }

    // 5. Toutes les autres images référencées (qui restent dans le set)
    for (const QString& imgFileName : referencedImages) {
        int idx = findFirstImageIndex(allImages, [&](const ImageInfo &im) {
            // Chercher dans les images du 1er niveau (racine)
            if (!im.parentName.isEmpty()
                && im.parentName != "."
                && im.parentName != srcDir.dirName()) {
                return false;  // Pas à la racine
            }

            return im.fileName.compare(imgFileName, Qt::CaseInsensitive) == 0;
        });

        if (idx >= 0) {
            addImageByAllIndex(result, allImages, idx, /*forceOpaquePrev=*/false);
        }
    }

    // ========== 6. Banques (sous-dossiers) dans l'ordre de font.json ==========

    QMap<QString, BankInfo> banks = buildBanks(allImages, srcDir);

    for (const QString &bankName : fontOrder) {
        if (bankName.compare("week", Qt::CaseInsensitive) == 0) {
            emitWeekBank(allImages, result);
            continue;
        }
        if (bankName.compare("month", Qt::CaseInsensitive) == 0) {
            emitMonthBank(allImages, result);
            continue;
        }

        if (banks.contains(bankName)) {
            emitBankInOrder(bankName, banks[bankName], allImages, result);
        }
    }

    return result;
}


// ==========================================================================
// 9. ÉCRITURE DU .IWF FINAL
// ==========================================================================

// écrit un name[32] nul-terminé, padded avec 0
static void writeName32(char dest[32], const QString &name)
{
    QByteArray latin = name.toLatin1();
    memset(dest, 0, 32);
    int n = std::min(31, static_cast<int>(latin.size()));
    memcpy(dest, latin.constData(), n);
    // '\0' déjà assuré par memset
}

bool createIwfFromFolder(const QDir &srcDir, const QString &outPath)
{
    // 1. Construire la playlist ordonnée
    QVector<EntryToPack> entries = buildOrderedEntryList(srcDir);
    const quint16 entryCount = static_cast<quint16>(entries.size());

    // 2. Convertir chaque entrée en blob + mémoriser taille/offset local
    struct BuiltEntry {
        EntryToPack meta;
        QByteArray  data;
        quint32     localDataOffset;
        quint32     size;
    };

    QVector<BuiltEntry> built;
    built.reserve(entries.size());

    quint32 runningDataOffset = 0;

    for (const EntryToPack &e : entries) {
        BuiltEntry b;
        b.meta = e;

        if (e.isImage) {
            // convertit PNG/BMP vers RAW VeryFit
            b.data = PngToVeryfitRaw::convertAuto(e.absPath, e.forceOpaquePreview);
        } else {
            QFile f(e.absPath);
            if (!f.open(QIODevice::ReadOnly)) {
                b.data = QByteArray();
            } else {
                b.data = f.readAll();
            }
        }

        b.size            = static_cast<quint32>(b.data.size());
        b.localDataOffset = runningDataOffset;

        runningDataOffset += b.size;
        built.push_back(b);
    }

    // 3. Construire header + table d'index

    quint32 headerSize = 8;
    quint32 indexSize  = static_cast<quint32>(entryCount) * 40;
    quint32 baseOffset = headerSize + indexSize;

    QByteArray headerAndIndex;
    headerAndIndex.resize(baseOffset);

    {
        uchar *p = reinterpret_cast<uchar*>(headerAndIndex.data());

        // magic
        p[0] = 'i';
        p[1] = 'w';
        p[2] = 'f';
        p[3] = 0x00;

        // version 1
        p[4] = 0x01;
        p[5] = 0x00;

        // nombre d'entrées
        p[6] = static_cast<uchar>(entryCount & 0xFF);
        p[7] = static_cast<uchar>((entryCount >> 8) & 0xFF);

        uchar *idxPtr = p + 8;

        for (int i = 0; i < built.size(); ++i) {
            const BuiltEntry &b = built[i];

            // name[32]
            writeName32(reinterpret_cast<char*>(idxPtr), b.meta.logicalName);
            idxPtr += 32;

            // offset absolu dans le fichier final :
            quint32 absOff = baseOffset + b.localDataOffset;
            *idxPtr++ = static_cast<uchar>( absOff        & 0xFF );
            *idxPtr++ = static_cast<uchar>((absOff >> 8 ) & 0xFF );
            *idxPtr++ = static_cast<uchar>((absOff >> 16) & 0xFF );
            *idxPtr++ = static_cast<uchar>((absOff >> 24) & 0xFF );

            // size
            quint32 sz = b.size;
            *idxPtr++ = static_cast<uchar>( sz        & 0xFF );
            *idxPtr++ = static_cast<uchar>((sz >> 8 ) & 0xFF );
            *idxPtr++ = static_cast<uchar>((sz >> 16) & 0xFF );
            *idxPtr++ = static_cast<uchar>((sz >> 24) & 0xFF );
        }
    }

    // 4. concat header+index + data blobs
    QByteArray finalIwf;
    finalIwf.reserve(headerAndIndex.size() + runningDataOffset);
    finalIwf.append(headerAndIndex);

    for (const BuiltEntry &b : built) {
        finalIwf.append(b.data);
    }

    // 5. écriture disque
    QFile outFile(outPath);
    if (!outFile.open(QIODevice::WriteOnly)) {
        return false;
    }
    qint64 wr = outFile.write(finalIwf);
    outFile.close();

    if (wr != finalIwf.size()) {
        return false;
    }

    return true;
}
