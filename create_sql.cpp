#include "create_sql.h"

#include <QFile>
//#include <QFileDialog>
//#include <QInputDialog>
//#include <QFileInfo>
//#include <QDir>
//#include <QMessageBox>
#include <QCryptographicHash>
#include <QSettings>



bool create_sql::exportCompassTextSql(const QString& sqlPath, const QString& shortName, const QByteArray& data)
{
    QFile outFile(sqlPath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Impossible d'ouvrir le fichier:" << sqlPath;
        return false;
    }
    QTextStream out(&outFile);
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    out.setEncoding(QStringConverter::Utf8);
#else
    out.setCodec("UTF-8");
#endif

    //displayPourCreationSql(content, QFileInfo(finalPath).fileName());
    QString contentStr = CreateTextHexaFromLz(data, shortName);

    out << "-- Auto-generated inserts (TEXT) for " << TABLE_TEXT << "\n";
    out << "SET NAMES utf8mb4;\n";
    out << "USE " << DB_NAME << ";\n";
    out << "START TRANSACTION;\n";

    const QString content   = contentStr;
    const QString rubId     = sqlEscape(shortName);         // ton identifiant texte
    const QString contentEsc= sqlEscape(content);

    out << "INSERT INTO " << TABLE_TEXT << " (RUB_ID, RUB_CADRAN) "
                                           "VALUES ('" << rubId << "', '" << contentEsc << "') "
                                            "ON DUPLICATE KEY UPDATE RUB_CADRAN=VALUES(RUB_CADRAN);\n";

    out << "COMMIT;\n";
    outFile.close();
    return true;
}


bool create_sql::exportCompassPngSql(const QString& sqlPath, const QString& shortName, const QString& pngFiles)
{
    QFile outFile(sqlPath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Impossible d'ouvrir le fichier:" << sqlPath;
        return false;
    }
    QTextStream out(&outFile);
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    out.setEncoding(QStringConverter::Utf8);
#else
    out.setCodec("UTF-8");
#endif

    out << "-- Auto-generated inserts (PNG -> MEDIUMBLOB) for " << TABLE_PNG << "\n";
    out << "SET NAMES utf8mb4;\n";
    out << "USE " << DB_NAME << ";\n";
    out << "START TRANSACTION;\n";

    // for (const QString& path : pngFiles) {
    QFile f(pngFiles);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "Lecture PNG impossible:" << pngFiles;
        return false;
    }
    const QByteArray bytes = f.readAll();
    f.close();

    const QString rubId   = sqlEscape(shortName);
    const QString mime    = sqlEscape("image/png");
    const int     size    = bytes.size();
    const QString shaHex  = QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()
        );
    const QString hexBody = QString::fromLatin1(bytes.toHex()); // 0x...

    out << "INSERT INTO " << TABLE_PNG
        << " (RUB_ID, RUB_MIME, RUB_PNG, RUB_SIZE_BYTES, RUB_SHA256) "
        << "VALUES ('" << rubId << "', '" << mime << "', 0x" << hexBody
        << ", " << size << ", '" << shaHex << "') "
        << "ON DUPLICATE KEY UPDATE "
           "RUB_MIME=VALUES(RUB_MIME), "
           "RUB_PNG=VALUES(RUB_PNG), "
           "RUB_SIZE_BYTES=VALUES(RUB_SIZE_BYTES), "
           "RUB_SHA256=VALUES(RUB_SHA256);\n";
    //  }

    out << "COMMIT;\n";
    outFile.close();
    return true;
}

QString create_sql::sqlEscape(const QString& in) {
    // Échapper backslashes puis apostrophes pour un littéral SQL sûr
    QString s = in;
    s.replace("\\", "\\\\");
    s.replace("'", "''");
    return s;
}

//*********************************************************************************************************
QString create_sql::CreateTextHexaFromLz(const QByteArray &data, QString filename)
//*********************************************************************************************************

{

    QStringList trames;
    QString fileName = filename + ".iwf.lz";
    QByteArray hex = fileName.toUtf8().toHex().toLower();

    for (int i = 0; i < data.size(); i += 177) {
        QByteArray chunk = data.mid(i, 177);
        trames << QString(chunk.toHex());
    }

    QString liste = trames.join("");  // conversion propre ici

    int nbCaracteres = liste.length();
    int nbOctets = nbCaracteres / 2;

    quint32 value = nbOctets;  // 0x0001319D en big endian

    QByteArray ba;
    QDataStream stream(&ba, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);  // DCBA
    stream << value;
    QString hexLittleEndian = ba.toHex().toLower();  // => "9D310100"


    trames.clear();
    for (int i = 0; i < data.size(); i += 177) {
        QByteArray chunk = data.mid(i, 177);
        trames << "d10200" + QString(chunk.toHex());

    }


    liste = trames.join("\n");  // conversion propre ici

    //QString shortName = filename.section(".iwf.lz", 0, 0);
    //QString entete = shortName + "\n\nd108ff5ee8070002" + hex + "0000\n";
    QString entete = "d108ff5ee8070002" + hex + "0000\n";
    entete += "d101ff" + hexLittleEndian + "02" + hex + "0000\n";
    entete += "d1050a\n";

    liste = entete + liste;
    liste = liste + "\nd10392f03000";

    return liste;

}

