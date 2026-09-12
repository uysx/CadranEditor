#ifndef CREATE_SQL_H
#define CREATE_SQL_H
#include <QString>


namespace {
constexpr char DB_NAME[]    = "ksixcadran2025";
constexpr char TABLE_TEXT[] = "ksix_table_cadran_compass"; //ksix_table_cadran_compass
constexpr char TABLE_PNG[]  = "ksix_table_image_cadran_compass"; //ksix_table_image_cadran_compass
}
class create_sql
{
public:

    static bool exportCompassTextSql(const QString &sqlPath, const QString& shortName, const QByteArray& lzcontent);
    static bool exportCompassPngSql(const QString &sqlPath, const QString& shortName, const QString &pngFiles);
private:
    static QString baseNameNoExtLimit64(const QString &path);
    static QString sqlEscape(const QString &in);
    static QString CreateTextHexaFromLz(const QByteArray &data, QString filename);
};

#endif // CREATE_SQL_H
