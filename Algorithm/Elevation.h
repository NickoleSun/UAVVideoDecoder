#ifndef ELEVATION_H
#define ELEVATION_H

#include <QObject>
#include <iostream>
#include <fstream>
#include <math.h>
using namespace std;


namespace AppService {
    class Elevation : public QObject
    {
        Q_OBJECT
    public:
        explicit Elevation(QObject *t_parent = nullptr);
        ~Elevation() override;
        QString folder() {return m_folder;}
        int resolution() {return m_resolution;}
        Q_INVOKABLE void setParams(QString t_folder, int t_resolution);
        Q_INVOKABLE float getElevation(float t_lat, float t_lon);

    public Q_SLOTS:

    protected:
        bool srtmLoadTile(const char* t_folder, int t_latDec, int t_lonDec, unsigned char* t_srtmTile);
        void srtmReadPx(int t_y, int t_x, int* t_height,unsigned char* t_srtmTile);
    private:
        QString m_folder;
        int m_resolution = 60;
        int m_secondsPerPx = 3;  //arc seconds per pixel (3 equals cca 90m)
        int m_totalPx = 1201;
        float m_lat = 0;
        float m_lon = 0;
    };
}

#endif // ELEVATION_H
