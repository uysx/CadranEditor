#ifndef JSONWATCHFACE_H
#define JSONWATCHFACE_H

#include <QString>
#include <QList>
#include <QImage>
#include <QJsonObject>


struct JsonItem {

    QString imagePath;       // Image associée localement
    QString widget;          // "custom", watch"ring", "progressbar", etc.

    // Position et dimensions
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;

    // Texte / Numérique
    QString type;            // "battery", "step", "icon", etc.
    QString align;           // "left", "center", etc.
    QString font;            // ex. "num_steps"
    int fontnum = 0;
    int style = 0;           // Parfois utilisé (ex: date, week, month)

    // Couleurs
    QString fgcolor;         // Couleur principale
    QString fgrender;        // Couleur secondaire ou effet
    QString bgcolor;         // Couleur de fond
    QString bgrender;        // Pour le fond (ex: pour progressbar)
    QString bg;              // Image de fond

    // Animation
    QString animaicon;       // Nom du dossier de frames
    int turn = 0;            // 0 ou 1 (animation sens ?)
    int frame = 0;           // nombre de frames
    int time = 0;            // durée de l’animation en ms
    QString animatype;       // "return", etc.
    int animabpp = 0;
    QString animaformat;

    // Progression (progressbar, anneau)
    QString progress;        // image de progression
    int startangle = 0;      // pour les anneaux
    int endangle = 0;
    int ringedge = 0;

    // Divers
    QString app;             // Rare
    int metricinch = 0;      // unité distance



    //"widget": "watch",
    //"type": "time",

    QString second;
    int seccenterx = 0;
    int seccentery = 0;
    int secanchorx = 0;
    int secanchory = 0;
    QString minute;
    int mincenterx = 0;
    int mincentery = 0;
    int minanchorx = 0;
    int minanchory = 0;
    QString hour;
    int hourcenterx = 0;
    int hourcentery = 0;
    int houranchorx = 0;
    int houranchory = 0;

};



class JsonWatchface {
public:
    bool read(const QString &jsonFilePath, const QString &baseDir);
    QImage getImage(int index) const;

    QList<JsonItem> items;
    QString basePath;
    QString headerText;
    QString backgroundImage;


};

//QJsonObject jsonFromItem(const JsonItem &item);


#endif // JSONWATCHFACE_H
