/**
*                                                                   
* @clacc TargetLocalization                                                      
*                                                                   
* Class estimate the gps position of target from the camera image.                                  
*                                                                   
* 2020 created by Dongnt11 @viettel.com.vn                                   
*/

#include <stdio.h>
#include <math.h>
#include "TargetLocalization.h"


AppService::TargetLocalization::TargetLocalization() :
Rvb(),
Rbg(),
Rgc(),
Rci(),
p_i(),
p_I(),
p_C()
{

}

AppService::TargetLocalization::~TargetLocalization()
{
	//dtor
}

void AppService::TargetLocalization::setSensorParams(float sx, float sy)
{
    printf("setSensorParams %f,%f\r\n",sx,sy);
    visionViewInit(sx,sy,1920,1080);
}

void AppService::TargetLocalization::visionViewInit(float pixel_sizeX, float pixel_sizeY, int image_col_pixel, int image_row_pixel)
{
	initializedVisionView = 1;
    camParams.pixelSizeX = pixel_sizeX;
    camParams.pixelSizeY = pixel_sizeY;
	camParams.imageHeight = image_row_pixel;
	camParams.imageWidth = image_col_pixel;

	// calculate the parameters
    if (pixel_sizeX <= 0.000001f)
	{
        camParams.Sx = 33.00f;
        camParams.Sy = 25.00f;
	}
    else
    {
        camParams.Sx = camParams.pixelSizeX;
        camParams.Sy = camParams.pixelSizeY;
    }
	camParams.offset_X = camParams.imageWidth / 2;
	camParams.offset_Y = camParams.imageHeight / 2;
	// limit the target distance to UAV 25km
	camParams.maxImageDepth = 25000.0f;

	camParams.d_ci(-camParams.imageHeight / 2 * camParams.Sy, camParams.imageWidth / 2 * camParams.Sx, 0.0f);
	camParams.d_bg(0.0f, 0.0f, 0.0f);

	Rgc = Matrix3f(0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f);    //Rgc.from_euler(0.0f, PI2, 0.0f);
	Rci = Matrix3f(0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);	  //Rci.from_euler(0.0f, 0.0f, PI2);
}
/*
*      Move gps position from 1 point to another point
*/
void AppService::TargetLocalization::movePositionFlatEarth (struct GpsPosition origPos, float pTrack_rad, float pDistance_m, struct GpsPosition &newPos)
{
	// calculate distance in lat and lon axis
	float distLat = pDistance_m * cosf (pTrack_rad); 
    float distLon = pDistance_m * sinf (pTrack_rad);

    float RD1 = DEG_2_RAD / scaleFactor;
    float DR3 = RAD_2_DEG * (scaleFactor / EARTH_RADIUS);
    float MAX_LAT = 89.0f * scaleFactor;

    //  Limitation to the latitude range <-89,+89>
    if (origPos._latitude > MAX_LAT || origPos._latitude < -MAX_LAT)
    {
        newPos = origPos;
    }

    //  latitude in radians
    float lat0 = static_cast<float>(origPos._latitude) * RD1;

    //  Meters conversion to degrees and scale
    int dLat = static_cast<int>(distLat * DR3);
    int dLon = static_cast<int>((distLon * DR3) / cosf (lat0));

	//  correctness control of the float
    bool ret1 = assure(distLat, 0.0f);
    bool ret2 = assure(distLon, 0.0f);

    // new coordinates
    newPos._longitude = origPos._longitude + dLon;
    newPos._latitude = origPos._latitude + dLat;

	newPos.Latitude = (double)(newPos._latitude) / scaleFactor;
	newPos.Longitude = (double)(newPos._longitude) / scaleFactor;
}

/*
* Distance between to point in map
*/
float AppService::TargetLocalization::distanceFlatEarth(struct GpsPosition fromPos, struct GpsPosition toPos)
{
    float RD1 = DEG_2_RAD / scaleFactor;
    float RD2 = (EARTH_RADIUS * DEG_2_RAD) / scaleFactor;

    float dLon = static_cast<float>(toPos.Longitude * scaleFactorD - fromPos.Longitude * scaleFactorD);
    float dLat = static_cast<float>(toPos.Latitude * scaleFactorD - fromPos.Latitude * scaleFactorD);

    float lat0 = static_cast<float>(fromPos.Latitude * scaleFactorD) * RD1;
	float cLat0 = cosf (lat0);
    float d = RD2 * sqrtf (dLat*dLat + dLon*dLon*cLat0*cLat0);

    return d;
}

/*
* Target location compute
*/
void AppService::TargetLocalization::targetLocationCompute(
        int target_x_ip, int target_y_ip, struct UavData* curUavData,
        struct GpsPosition &targetPoint,
        float &dTrack, float &dElevator)
{
	p_i(target_x_ip * camParams.Sx, target_y_ip * camParams.Sy, curUavData->opticalLength);

	Rvb.from_euler(curUavData->rollUAV, curUavData->pitchUAV, curUavData->yawUAV);
	Rbg.from_euler(curUavData->rollCam, curUavData->tiltCam, curUavData->panCam);

    p_I = Rvb.transposed() * (Rbg.transposed() * Rgc.transposed() * (Rci.transposed() * p_i - camParams.d_ci) - camParams.d_bg * 10000000.0f);
    p_C = Rvb.transposed() * -camParams.d_bg * 10000000.0f;

	Vector3f seeVec = p_I - p_C;
    if (seeVec.z < 1000.0f)
        seeVec.z = 1000.0f;

    float x = seeVec.x / seeVec.z * (curUavData->UavPosition.Altitude - p_C.z / 10000000.0f);
    float y = seeVec.y / seeVec.z * (curUavData->UavPosition.Altitude - p_C.z / 10000000.0f);
    float z = curUavData->UavPosition.Altitude;

    float dDistance = sqrtf(x * x + y * y);
    dTrack = atan2(y, x);
    dElevator = atan2(-z, dDistance);

    //terrainElevationCorrection(curUavData, dTrack, dDistance);
	// calculate the position of target
    movePositionFlatEarth(curUavData->UavPosition, dTrack, dDistance, targetPoint);

    //targetPoint.terrainElevation = m_elevationFinder.getElevation(targetPoint.Latitude, targetPoint.Longitude);
}

/*
* Calculate elevation and azimuth compute
*/
void AppService::TargetLocalization::lightOfSightCompute(
        int target_x_ip, int target_y_ip, struct UavData* curUavData,
        float &dTrack, float &dElevator)
{
    p_i(target_x_ip * camParams.Sx, target_y_ip * camParams.Sy, curUavData->opticalLength);

    Rvb.from_euler(curUavData->rollUAV, curUavData->pitchUAV, curUavData->yawUAV);
    Rbg.from_euler(curUavData->rollCam, curUavData->tiltCam, curUavData->panCam);

    p_I = Rvb.transposed() * (Rbg.transposed() * Rgc.transposed() * (Rci.transposed() * p_i - camParams.d_ci) - camParams.d_bg * 10000000.0f);
    p_C = Rvb.transposed() * -camParams.d_bg * 10000000.0f;

    Vector3f seeVec = p_I - p_C;

    float seeVecXY = sqrtf(seeVec.x * seeVec.x + seeVec.y * seeVec.y);
    dTrack = atan2(seeVec.y, seeVec.x);
    dElevator = atan2(-seeVec.z, seeVecXY);
}
/*
* Target location main
*/

void AppService::TargetLocalization::targetLocationMain(int x_ip, int y_ip, float hfov_rad, float roll_rad, float pitch_rad, float yaw_rad,
    float pan_rad, float tilt_rad, float rollCam_rad, double uav_lat_deg, double uav_lon_deg, float uav_alt_m)
{
	// update current data of uav
	UavDataState.hFov			= hfov_rad;
	UavDataState.opticalLength	= camParams.Sx * camParams.imageWidth / (2.0f * tanf(hfov_rad / 2.0f));
	UavDataState.panCam			= pan_rad;
    UavDataState.tiltCam		= tilt_rad;
    UavDataState.rollCam        = rollCam_rad;

	UavDataState.rollUAV		= roll_rad;
	UavDataState.pitchUAV		= pitch_rad;
	UavDataState.yawUAV			= yaw_rad;

    UavDataState.UavPosition.Latitude	= uav_lat_deg;
    UavDataState.UavPosition.Longitude	= uav_lon_deg;
//    UavDataState.UavPosition.terrainElevation = m_elevationFinder.getElevation(uav_lat_deg, uav_lon_deg);
    UavDataState.UavPosition.Altitude	= uav_alt_m;// -  UavDataState.UavPosition.terrainElevation;


	UavDataState.UavPosition._latitude	= (int)(uav_lat_deg * scaleFactorD);
	UavDataState.UavPosition._longitude	= (int)(uav_lon_deg * scaleFactorD);

	// calculate the position of target
    targetLocationCompute(x_ip, y_ip, &UavDataState, UavDataState.TargetPosition,
                          azimuth, elevator);
}

/*
* Calculate elevation and azimuth main
*/
void AppService::TargetLocalization::CalLightOfSightMain(int x_ip, int y_ip, float hfov_rad, float roll_rad, float pitch_rad, float yaw_rad,
    float pan_rad, float tilt_rad, float rollCam_rad)
{
    // update current data of uav
    UavDataState.hFov			= hfov_rad;
    UavDataState.opticalLength	= camParams.Sx * camParams.imageWidth / (2.0f * tanf(hfov_rad / 2.0f));
    UavDataState.panCam			= pan_rad;
    UavDataState.tiltCam		= tilt_rad;
    UavDataState.rollCam        = rollCam_rad;

    UavDataState.rollUAV		= roll_rad;
    UavDataState.pitchUAV		= pitch_rad;
    UavDataState.yawUAV			= yaw_rad;

    // calculate the light of sight
    lightOfSightCompute(x_ip, y_ip, &UavDataState, azimuth, elevator);
}

/*
* Estimate the terrain elevation at the target location and Correct the dDistance and dTrack
*/
void AppService::TargetLocalization::terrainElevationCorrection(struct UavData* curUavData, float dDistance, float dTrack)
{
	struct GpsPosition targetPosEst;
    float lightOfSightGain = -curUavData->UavPosition.Altitude/ dDistance;
    float lightOfSightBias = curUavData->UavPosition.Altitude;
	float newdDistance = dDistance;

	// An auxiliary variable depicted: terrain elevation at Uav - terrain elevation at 
	float targetAltitude = 0.0f;
	float deltaElevation = 0.0f;
	int j;

	// A new agorithm
    for (j = 0; j < 5; j++)
	{
		movePositionFlatEarth(curUavData->UavPosition, dTrack, newdDistance, targetPosEst);
        targetPosEst.terrainElevation = m_elevationFinder.getElevation(targetPosEst.Latitude, targetPosEst.Longitude);
		deltaElevation = targetPosEst.terrainElevation - curUavData->UavPosition.terrainElevation;

        if ((targetAltitude - deltaElevation) * targetPosEst.terrainElevation >= 0)
        {
            dDistance = (dDistance + newdDistance) / 2;
            break;
        }
        dDistance = newdDistance;
        newdDistance = (targetPosEst.terrainElevation - lightOfSightBias) / lightOfSightGain;
        targetAltitude = deltaElevation;
    }
	// Bisection method: enhance the accuracy of the estimation
	// But unless the new agorithm completes, this bisection method would not operate
	// and report: "Error Target Localization: Number of loops" 
    if (j == 5)
    {
		dDistance = 0.0f;
		return;
	}
	float Distance;
    int s;
    for (s = 0; s < 5; s++)
	{
		Distance = (dDistance + newdDistance) / 2;

		movePositionFlatEarth(curUavData->UavPosition, dTrack, Distance, targetPosEst);
        targetPosEst.terrainElevation = m_elevationFinder.getElevation(targetPosEst.Latitude, targetPosEst.Longitude);
		deltaElevation = targetPosEst.terrainElevation - curUavData->UavPosition.terrainElevation;

		targetAltitude = lightOfSightGain * Distance + lightOfSightBias;
		if ((targetAltitude - deltaElevation) * targetPosEst.terrainElevation > 0)
		{
			newdDistance = Distance;
		}
		else
		{
			dDistance = Distance;
		}
        if (fabs(dDistance - newdDistance) < 90.0f)
		{
			dDistance = newdDistance;
            break;
		}
    }
}


