#include "Elevation.h"

AppService::Elevation::Elevation(QObject* t_parent) : QObject(t_parent)
{

}

AppService::Elevation::~Elevation()
{

}

bool AppService::Elevation::srtmLoadTile(const char *folder, int latDec, int lonDec, unsigned char * srtmTile)
{
    char filename[256];
    sprintf(filename, "%s/%c%02d%c%03d.hgt", folder,
                latDec>0?'N':'S', abs(latDec),
                lonDec>0?'E':'W', abs(lonDec));
#ifdef DEBUG
    printf("Opening %s\n", filename);
#endif
    FILE* srtmFd = fopen(filename, "r");
    if(srtmFd == nullptr) {
#ifdef DEBUG
        printf("Error opening %s\n",  filename);
#endif
        return false;
    }else{
        fread(srtmTile, 1, static_cast<size_t>(2 * m_totalPx * m_totalPx), srtmFd);
        fclose(srtmFd);
        return true;
    }
}

void AppService::Elevation::srtmReadPx(int y, int x, int* height,unsigned char * srtmTile)
{
    int row = (m_totalPx-1) - y;
    int col = x;
    int pos = (row * m_totalPx + col) * 2;

    //set correct buff pointer
    unsigned char * buff = & srtmTile[pos];

    //solve endianity (using int16_t)
    int16_t hgt = static_cast<int16_t>(0 | (buff[0] << 8) | (buff[1] << 0)) ;

    *height = static_cast<int>(hgt);
}

void AppService::Elevation::setParams(QString t_folder, int t_resolution)
{
    m_folder = t_folder;
    m_resolution = t_resolution;
}

float AppService::Elevation::getElevation(float t_lat, float t_lon)
{
    float elevation = 0;
#ifdef DEBUG
    printf("getElevation (%f,%f) from %s\r\n",t_lat,t_lon,m_folder.toStdString().c_str());
#endif
    int latDec = static_cast<int>(floor(t_lat));
    int lonDec = static_cast<int>(floor(t_lon));

    float secondsLat = (t_lat-latDec) * m_resolution * m_resolution;
    float secondsLon = (t_lon-lonDec) * m_resolution * m_resolution;
    unsigned char* srtmTile = new unsigned char[2 * m_totalPx * m_totalPx];
    if(srtmLoadTile(m_folder.toStdString().c_str(), latDec, lonDec,srtmTile) == false){
        delete[] srtmTile;
        return elevation;
    }

    //X coresponds to x/y values,
    //everything easter/norhter (< S) is rounded to X.
    //
    //  y   ^
    //  3   |       |   S
    //      +-------+-------
    //  0   |   X   |
    //      +-------+-------->
    // (sec)    0        3   x  (lon)

    //both values are 0-1199 (1200 reserved for interpolating)
    int y = static_cast<int>(secondsLat/m_secondsPerPx);
    int x = static_cast<int>(secondsLon/m_secondsPerPx);

    //get norther and easter points
    int height[4];
    srtmReadPx(y,   x, &height[2],srtmTile);
    srtmReadPx(y+1, x, &height[0],srtmTile);
    srtmReadPx(y,   x+1, &height[3],srtmTile);
    srtmReadPx(y+1, x+1, &height[1],srtmTile);

    //ratio where X lays
    float dy = static_cast<float>(fmod(secondsLat, m_secondsPerPx) / m_secondsPerPx);
    float dx = static_cast<float>(fmod(secondsLon, m_secondsPerPx) / m_secondsPerPx);

    // Bilinear interpolation
    // h0------------h1
    // |
    // |--dx-- .
    // |       |
    // |      dy
    // |       |
    // h2------------h3
    delete[] srtmTile;
    elevation =  height[0] * dy * (1 - dx) +
            height[1] * dy * (dx) +
            height[2] * (1 - dy) * (1 - dx) +
            height[3] * (1 - dy) * dx;
    return elevation;
}
