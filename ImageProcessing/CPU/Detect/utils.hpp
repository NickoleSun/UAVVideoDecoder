#ifndef UTILS_H
#define UTILS_H

#include <opencv2/opencv.hpp>
#include <iostream>
class GSauvola
{
public:
    GSauvola()
    {
        // Get input params from config file and check validation
        width           = 128;
        height          = 128;
        // ~~~> Checked before initialize new object
        minRadius       = 4;
        if (minRadius < 4)
        {
            std::cerr << "WARNING: Radius of object is too small." << std::endl;
            minRadius = 4;
        }

        maxRadius       = 24;
        if (minRadius > 24)
        {
            std::cerr << "WARNING: Radius of object is too large." << std::endl;
            minRadius = 16;
        }

        defaultWidth    = 8;
        defaultHeight   = 8;
        // ~~~> Checked before initialize new object
        winSize         = 3;
        if(winSize < 3)
        {
            std::cerr << "WARNING: Window size must be greater than 2 pixel." << std::endl;
            winSize = 3;
        }
        if(winSize % 2 == 0)
        {
            winSize--;
        }

        minAreaRate   = 0.25;
        if(minAreaRate <= 0.2f)
            minAreaRate = 0.2f;

        maxDimensionRate = 3.;
        if(maxDimensionRate < 1.f)
            maxDimensionRate = 1.;

        sizeWeightContribFactor = 0.25f;
        if(sizeWeightContribFactor < 0.f)
        {
            std::cerr << "WARNING: Invalid value. The weight must be not less than 0 or greater than 1." << std::endl;
            sizeWeightContribFactor = 0.f;
        }
        if(sizeWeightContribFactor > 1.f)
        {
            std::cerr << "WARNING: Invalid value. The weight must be not less than 0 or greater than 1." << std::endl;
            sizeWeightContribFactor = 1.f;
        }

        locationWeightContribFactor = 0.75f;
        if(locationWeightContribFactor < 0.f)
        {
            std::cerr << "WARNING: Invalid value. The weight must be not less than 0 or greater than 1." << std::endl;
            locationWeightContribFactor = 0.f;
        }
        if(locationWeightContribFactor > 1.f)
        {
            std::cerr << "WARNING: Invalid value. The weight must be not less than 0 or greater than 1." << std::endl;
            locationWeightContribFactor = 1.f;
        }

        if (sizeWeightContribFactor + locationWeightContribFactor != 1.f)
        {
            std::cerr << "Sum of weights must be equal to 1." << std::endl;
            locationWeightContribFactor = 1.f - sizeWeightContribFactor;
        }
    }
    ~GSauvola(){}

    std::vector<cv::Rect> detectObject(cv::Mat &inputFrame, cv::Point clickPoint, cv::Size initSize);
    void binarize(const cv::Mat input, cv::Mat &output, const int winSize, const bool padding);

private:
    void pad(const cv::Mat input, cv::Mat &output, const int paddingSize, const int paddingValue);

private:
    // Get input params from config file and check validation
    int width;                             /**< width of search region >*/
    int height;                            /**< height of search region >*/
    int minRadius;                         /**< minimal radius of object that can be detected >*/
    int maxRadius;                         /**< maximal radius of object that can be detected >*/
    int defaultWidth;                      /**< default width of target when there is no object detected >*/
    int defaultHeight;                     /**< default height of target when there is no object detected >*/
    int winSize;                           /**< size of slide window used in binary algorithm >*/

    float minAreaRate;                     /**< minimal rate of the contour area over the bounding box area in order to be selected as an object >*/
    float maxDimensionRate;                /**< maximal rate of 2 sides of the bounding box in order to be selected as an object >*/
    float sizeWeightContribFactor;         /**< weight of object-size-factor when it is used in order to calculate combined weight >*/
    float locationWeightContribFactor;     /**< weight of object-position-factor when it is used in order to calculate combined weight >*/
};

class CNExtractor
{
public:
    CNExtractor();
    ~CNExtractor(){}
    void extractCN(const cv::Mat input, cv::Mat& output);

private:
     void loadCNModel();
     cv::Mat divideByNumber(cv::Mat inMat, int number);

private:
     cv::Mat m_cnModel;
     std::vector<std::vector<float>> colorVec;
};

#endif // UTILS_H
