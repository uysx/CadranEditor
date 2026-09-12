#include "jsonwatchface.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDir>
bool JsonWatchface::read(const QString &jsonFilePath, const QString &baseDir) {
    QFile file(jsonFilePath);
    if (!file.open(QIODevice::ReadOnly)) return false;

    basePath = baseDir;
    QByteArray jsonData = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (!doc.isObject()) return false;

    QJsonObject root = doc.object();  // ✅

    // ⬇️ Enregistrement des champs d'en-tête dans des attributs (ou retourne-les directement)
    QString entete;
    //entete += "{\n";
    entete += QString("  \"version\": %1\n").arg(root.value("version").toInt());
    entete += QString("  \"clouddialversion\": %1\n").arg(root.value("clouddialversion").toInt());
    entete += QString("  \"preview\": \"%1\"\n").arg(root.value("preview").toString());
    entete += QString("  \"name\": \"%1\"\n").arg(root.value("name").toString());
    entete += QString("  \"author\": \"%1\"\n").arg(root.value("author").toString());
    entete += QString("  \"description\": \"%1\"\n").arg(root.value("description").toString());
    entete += QString("  \"bkground\": \"%1\"\n").arg(root.value("bkground").toString());
    //entete += "}";

    // Sauvegarde temporaire dans un champ QString de la classe JsonWatchface
    this->headerText = entete;

    backgroundImage = root["bkground"].toString();  // ✅ "files0.png"

    QJsonArray arr = root["item"].toArray();  // ✅


        items.clear();


        for (const QJsonValue &val : arr) {
            QJsonObject obj = val.toObject();

            JsonItem item;

            item.widget     = obj.value("widget").toString();
            item.type       = obj.value("type").toString();
            item.align      = obj.value("align").toString();
            item.font       = obj.value("font").toString();
            //item.fontnum    = obj.value("fontnum").toInt();
            // item.style      = obj.value("style").toInt();

            item.fgcolor    = obj.value("fgcolor").toString();
            item.fgrender   = obj.value("fgrender").toString();
            item.bgcolor    = obj.value("bgcolor").toString();
            item.bgrender   = obj.value("bgrender").toString();
            item.bg         = obj.value("bg").toString();

            item.animaicon  = obj.value("animaicon").toString();
            // item.turn       = obj.value("turn").toInt();
            // item.frame      = obj.value("frame").toInt();

            item.animatype  = obj.value("animatype").toString();
            item.animaformat  = obj.value("animaformat").toString();


            item.progress   = obj.value("progress").toString();
            // item.startangle = obj.value("startangle").toInt();
            // item.endangle   = obj.value("endangle").toInt();
            // item.ringedge   = obj.value("ringedge").toInt();

            item.app        = obj.value("app").toString();
            // item.metricinch = obj.value("metricinch").toInt();

            item.x          = obj.value("x").toInt();
            item.y          = obj.value("y").toInt();
            item.w          = obj.value("w").toInt();
            item.h          = obj.value("h").toInt();

            item.fontnum = obj.contains("fontnum") ? obj.value("fontnum").toInt() : -1;
            item.style   = obj.contains("style")   ? obj.value("style").toInt()   : -1;
            item.turn    = obj.contains("turn")    ? obj.value("turn").toInt()    : -1;
            item.frame   = obj.contains("frame")   ? obj.value("frame").toInt()   : -1;
            item.time    = obj.contains("time")    ? obj.value("time").toInt()    : -1;
            item.animabpp = obj.contains("animabpp") ? obj.value("animabpp").toInt() : -1;

            item.startangle = obj.contains("startangle") ? obj.value("startangle").toInt() : -1;
            item.endangle   = obj.contains("endangle")   ? obj.value("endangle").toInt()   : -1;
            item.ringedge   = obj.contains("ringedge")   ? obj.value("ringedge").toInt()   : -1;
            item.metricinch = obj.contains("metricinch") ? obj.value("metricinch").toInt() : -1;

            item.hour        = obj.value("hour").toString();
            item.hourcenterx = obj.contains("hourcenterx") ? obj.value("hourcenterx").toInt() : -1;
            item.hourcentery = obj.contains("hourcentery") ? obj.value("hourcentery").toInt() : -1;
            item.houranchorx = obj.contains("houranchorx") ? obj.value("houranchorx").toInt() : -1;
            item.houranchory = obj.contains("houranchory") ? obj.value("houranchory").toInt() : -1;

            item.minute        = obj.value("minute").toString();
            item.mincenterx = obj.contains("mincenterx") ? obj.value("mincenterx").toInt() : -1;
            item.mincentery = obj.contains("mincentery") ? obj.value("mincentery").toInt() : -1;
            item.minanchorx = obj.contains("minanchorx") ? obj.value("minanchorx").toInt() : -1;
            item.minanchory = obj.contains("minanchory") ? obj.value("minanchory").toInt() : -1;

            item.second        = obj.value("second").toString();
            item.seccenterx = obj.contains("seccenterx") ? obj.value("seccenterx").toInt() : -1;
            item.seccentery = obj.contains("seccentery") ? obj.value("seccentery").toInt() : -1;
            item.secanchorx = obj.contains("secanchorx") ? obj.value("secanchorx").toInt() : -1;
            item.secanchory = obj.contains("secanchory") ? obj.value("secanchory").toInt() : -1;

            items.append(item);

        }



    return true;
}

QImage JsonWatchface::getImage(int index) const {
    if (index < 0 || index >= items.size()) return QImage();

    QString fullPath = QDir(basePath).filePath(items[index].imagePath);
    QImage img(fullPath);
    return img;
}

