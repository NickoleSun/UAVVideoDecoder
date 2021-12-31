/**
*                                                                   
* @clacc TargetLocalization                                                      
*                                                                   
* Class estimate the gps position of target from the camera image.                                  
*                                                                   
* 2020 created by Dongnt11 @viettel.com.vn                                   
*/

#ifndef TARGETLOCALIZATION_H
#define TARGETLOCALIZATION_H

#include "../AP_Math/AP_Math.h"
#include "../Elevation.h"

#ifndef RAD_2_DEG
#define RAD_2_DEG  (180.0f / M_PI)
#endif
#ifndef DEG_2_RAD
#define DEG_2_RAD  (M_PI / 180.0f)
#endif
// Data from UAv for calculation


/*
* Input of this function is the coordination of object by (m) on image with the origin of coordination at center of image
*/
namespace AppService {
    class TargetLocalization
    {
    public:
        // GPS Position
        struct GpsPosition
        {
            double Latitude;
            double Longitude;
            float Altitude;
            float terrainElevation;

            int _latitude;
            int _longitude;
        };

        // Params for uav data
        struct visionViewParam
        {
            // Pixel Size
            float pixelSizeX;	// [m]
            float pixelSizeY;	// [m]

            // Camera image scaler
            float Sx;			// [m/pixel] * 10000000
            float Sy;			// [m/pixel] * 10000000

            // Offsets of optical center of camera
            int offset_X;		// [pixels]
            int offset_Y;		// [pixels]

            // image size
            int imageWidth;			// width of image frame [pixel]
            int imageHeight;		// height of image frame [pixel]
            float maxImageDepth;

            Vector3f d_bg;			// distance from the body frame origin to the gimbal frame origin	[m]
            Vector3f d_ci;			// distance from the camera frame origin to the image frame origin  [m] (opticalLength)
        };

        struct UavData
        {
            struct GpsPosition UavPosition, TargetPosition;

            // attitude of UAV
            float rollUAV;			// [rad]
            float pitchUAV;			// [rad]
            float yawUAV;			// [rad]

            // attitude of Camera
            float panCam;			// [rad]
            float tiltCam;			// [rad]
            float rollCam;			// [rad]

            // optical length
            float opticalLength;	// [m] * 10000000
            float hFov;				// [rad]

        };

        TargetLocalization();
        ~TargetLocalization();
        void setSensorParams(float sx, float sy);
        void setParams(QString t_folder, int t_resolution) {m_elevationFinder.setParams(t_folder,t_resolution);}
        double getTargetLat(void){ return UavDataState.TargetPosition.Latitude; }
        double getTargetLon(void){ return UavDataState.TargetPosition.Longitude; }
        double getTargetAlt(void){ return UavDataState.TargetPosition.terrainElevation;}
        float getTargetElevator(void){ return elevator; }
        float getTargetAzimuth(void){ return azimuth; }

        void targetLocationMain(int x_ip, int y_ip, float hfov_rad, float roll_rad, float pitch_rad, float yaw_rad,
                        float pan_rad, float tilt_rad, float rollCam_rad, double uav_lat_deg, double uav_lon_deg, float uav_alt_m);

        void CalLightOfSightMain(int x_ip, int y_ip, float hfov_rad, float roll_rad, float pitch_rad, float yaw_rad,
                        float pan_rad, float tilt_rad, float rollCam_rad);

        void visionViewInit(float pixel_sizeX, float pixel_sizeY, int image_col_pixel, int image_row_pixel);
        float distanceFlatEarth (struct GpsPosition fromPos, struct GpsPosition toPos);

    private:
        // check valid data for a number
        bool isValid (float f)
        {
            return (fabsf(f) < FLT_MAX);
        }
        bool assure (float& f, float altValue)
        {
            if (isValid (f))
                return true;

            f = altValue;
            return false;
        }

        // min max of 2 float
        float compareMaxF (float in1, float in2)
        {
            return (in1 > in2) ? in1 : in2;
        }

        float compareMinF (float in1, float in2)
        {
            return (in1 < in2) ? in1 : in2;
        }
        // rotation matrix

        float scaleFactor = 10000000.0f;		// scale factor for internal use
        int scaleFactorI = 10000000;			//  scale factor for internal use - used in expressions with int type to avoid conversion
        double scaleFactorD = 10000000;					//  scale factor for internal use - used in expressions with double type to avoid conversion
        float EARTH_RADIUS = 6372797.560856f;   //  Earth radius in meters.

        int initializedVisionView;

        void movePositionFlatEarth (struct GpsPosition origPos, float pTrack_rad, float pDistance_m, struct GpsPosition &newPos);

        void targetLocationCompute (int target_x_ip, int target_y_ip, struct UavData* curUavData,
            struct GpsPosition &targetPoint,
            float &dTrack, float &dElevator);
        void lightOfSightCompute(int target_x_ip, int target_y_ip, struct UavData* curUavData,
            float &dTrack, float &dElevator);

        void terrainElevationCorrection(struct UavData* curUavData, float dDistance, float dTrack);


        struct UavData UavDataState;			// UAV and camera data
        struct visionViewParam camParams;		// Camera parameter
        float azimuth;                          // Target azimuth angle (NED coordination) (rad)
        float elevator;                         // Target elevator angle (NED coordination) (rad)

        Matrix3f Rvb;							// DCM from the vehicle frame to body frame		[rollUAV][pitchUAV][yawUAV]
        Matrix3f Rbg;							// DCM from the body frame to gimbal frame		[rollCam][tiltCam][panCam]
        Matrix3f Rgc;							// DCM from the gimbal frame to camera frame	[0.0][PI/2][0.0]
        Matrix3f Rci;							// DCM from the camera frame to image frame		[0.0][0.0][PI/2]

        Vector3f p_i;							// The position of target in image frame
        Vector3f p_I;							// The position of target in Inertial frame
        Vector3f p_C;							// The position of camera in Inertial frame

        AppService::Elevation m_elevationFinder;
    };
}
#endif  // TARGETLOCALIZATION_H
