#ifndef TRACKER_H
#define TRACKER_H

#include <iostream>
#include <opencv2/opencv.hpp>
#include <math.h>
//#define STD_SIZE    100
#define CELL_SIZE   4
#ifndef TRACK_LOST
    #define TRACK_LOST -2
#endif
#ifndef TRACK_INVISION
    #define TRACK_INVISION 0
#endif
#ifndef TRACK_OCCLUDED
    #define TRACK_OCCLUDED -1
#endif
#ifndef PI
#define PI    CV_PI
#endif
//Struct that contains image frame and its relatives
struct image_track{
    cv::Mat real_image;
    cv::Mat image_spectrum;
    cv::Mat filter_output;
    cv::Mat hog_feature;

    int cols;
    int rows;

    int opti_dft_comp_rows;
    int opti_dft_comp_cols;
};

//Struct ROI, tracked object
struct ROI_track{
    cv::Rect ROI;
    cv::Point ROI_center;
};

class Tracker
{
public:
    Tracker();
    ~Tracker();

    void initTrack(const cv::Mat &input_image, cv::Rect input_rect);
    void performTrack(const cv::Mat &input_image);
    void resetTrack();
    cv::Rect getPosition() const;
    int getState();
    bool isInitialized();
    bool getInitStatus();
    void ComputeDFT(image_track &input_image, bool preprocess);
    cv::Mat ComputeDFT(const cv::Mat &input_image, bool preprocess);
    void setROI(cv::Rect input_roi);
    void initFilter();
    void maskDesiredG(cv::Mat &output, int u_x, int u_y, double sigma=2, bool norm_energy=true);
    cv::Mat createEps(const cv::Mat &input_, double std=0.00001);
    void affineTransform(const cv::Mat &input_image, const cv::Mat &input_image2, cv::Mat &aff_img, cv::Mat &aff_img2);
    void dftDiv(const cv::Mat &dft_a, const cv::Mat &dft_b, cv::Mat &output_dft);

    cv::Point PerformTrack();
    float computePSR(const cv::Mat &correlation_mat);
    void update(cv::Point new_location);
    void updateFilter();
    void updateRoi(cv::Point new_center, bool scale_rot);

    cv::Mat extractFeatures(cv::Mat &patch);
    cv::Mat extractFeatures(image_track &input_image);
    void createHannWindow( cv::Mat &_hann, int *size_path);

private:
    image_track m_prevImg;
    image_track m_currImg;
    ROI_track m_prevRoi;
    ROI_track m_currRoi;

    bool m_initTrack;
    cv::Size m_imgSize;
    int m_trackSize;
    int m_stdSize;

    int m_state;
    double m_PSRRatio[2];
    double m_learningRate;
    int m_PSRMask;
    bool m_haveEps;

    cv::Mat m_filter;
    cv::Mat m_hanningWindow;

    cv::Mat m_hanWin;
    int m_featureMapSize[3];
};

#endif // TRACKER_H
