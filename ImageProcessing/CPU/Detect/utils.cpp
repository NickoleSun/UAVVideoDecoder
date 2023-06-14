#include "utils.hpp"
#include <chrono>

std::vector<cv::Rect> GSauvola::detectObject(cv::Mat &inputFrame, cv::Point clickPoint, cv::Size initSize)
{
    if(clickPoint.x - width / 2 < 0 || clickPoint.y - height / 2 < 0 ||
            clickPoint.x + width / 2 >= inputFrame.cols || clickPoint.y + height / 2 >= inputFrame.rows)
    {
        return {};
    }

    // Declare temporal variables using in this function
    cv::Mat                 grayPatch, binImg;
    std::vector<cv::Rect>   finalRect, _finalRect;
    std::vector<float>      combinedWeights;
    std::vector<float>      sizeWeights;
    std::vector<float>      locationWeights;

    // Search region
    cv::Rect roi;
    roi.x = clickPoint.x - width / 2;
    roi.y = clickPoint.y - height / 2;
    roi.width = width;
    roi.height = height;

//    std::cout << "Roi size: tl() =  [" << roi.x << ", " << roi.y << "] and br() = [" << roi.x + roi.width - 1 << ", " << roi.y + roi.height - 1 << "]\r\n";
    // Check input image data
    if(inputFrame.empty())
    {
        std::cerr << "No input image\r\n";
        _finalRect.push_back(cv::Rect(clickPoint.x - initSize.width / 2, clickPoint.y - initSize.height / 2, initSize.width, initSize.height));
        return _finalRect;
    }
    grayPatch = inputFrame(roi).clone();

    if(grayPatch.empty())
    {
        std::cerr << "ERROR: Cannot get image patch.\r\n";
        _finalRect.push_back(cv::Rect(clickPoint.x - initSize.width / 2, clickPoint.y - initSize.height / 2, initSize.width, initSize.height));
        return _finalRect;
    }

    if(grayPatch.channels() == 3)
    {
        cv::cvtColor(grayPatch, grayPatch, cv::COLOR_BGR2GRAY);
    }

//  Check conditions of search region if region size is not constant
//    if(width < 16 || height < 16)
//    {
//        std::cerr << "WARNING: Target is too small!!!\r\n";
//        _finalRect.push_back(cv::Rect(clickPoint.x - initSize.width / 2, clickPoint.y - initSize.height / 2, initSize.width, initSize.height));
//        return _finalRect;
//    }
//            else if((width > 32 && width <= 64) || (height > 32 && width <= 64))
//            {
//                cv::resize(patchImg, patchImg, cv::Size(), 0.5, 0.5);
//            }
//            else if((width > 64 && width <= 128) || (height > 64 && width <= 128))
//            {
//                cv::resize(grayPatch, grayPatch, cv::Size(), 0.5, 0.5);
//            }

//    else if(width > 128 || height > 128)
//    {
//        std::cerr << "WARNING: Target is too large!!!\r\n";
//        _finalRect.push_back(cv::Rect(clickPoint.x - initSize.width / 2, clickPoint.y - initSize.height / 2, initSize.width, initSize.height));
//        return _finalRect;
//    }

    std::cout << "Starting...\r\n";
//    auto start = std::chrono::high_resolution_clock::now();
    binarize(grayPatch, binImg, winSize, true);
//    auto end = std::chrono::high_resolution_clock::now();
//    std::cout << "binarize time: " << std::chrono::duration<double, std::milli>(end - start).count() << std::endl;

    // Find contour
    cv::Mat cannyImg;
    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;

    cannyImg = ~binImg;
    cv::findContours(cannyImg, contours, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    // Convert to rect
    std::vector<std::vector<cv::Point>> contours_poly(contours.size());
    std::vector<cv::Rect> boundRect(contours.size());
    std::vector<cv::Point2f> centers(contours.size());
    std::vector<float> radius(contours.size());

    for(size_t i = 0; i < contours.size(); i++)
    {
        cv::approxPolyDP(contours[i], contours_poly[i], 3, true);
        boundRect[i] = cv::boundingRect(contours_poly[i]);
        cv::minEnclosingCircle(contours_poly[i], centers[i], radius[i]);
    }

    // Filter boundingboxes and assign weights to them
    // Diagonal of initial rectangle
    float diagSize = static_cast<float>(std::sqrt(initSize.width * initSize.width + initSize.height * initSize.height));
    // Radius of search region
    float r = std::sqrt(static_cast<float>(roi.width * roi.width + roi.height * roi.height)) / 2.f;

    size_t bestIdx = 0;
    size_t counter = 0;
    for(size_t i = 0; i < contours.size(); i++)
    {
        if(radius[i] >= minRadius && radius[i] <= maxRadius &&
           double(cv::contourArea(contours[i])) > minAreaRate * boundRect[i].area() &&
           !(boundRect[i].width > maxDimensionRate * boundRect[i].height || boundRect[i].height > maxDimensionRate * boundRect[i].width)/* &&
           radius[i] <= 0.7 * static_cast<float>(initSize.width) &&
           radius[i] >= static_cast<float>(initSize.width) / 0.7*/)
        {
            int w = boundRect[i].width;
            int h = boundRect[i].height;
            int x = roi.x + boundRect[i].x + boundRect[i].width / 2;
            int y = roi.y + boundRect[i].y + boundRect[i].height / 2;

            finalRect.push_back(cv::Rect(boundRect[i].x + roi.x, boundRect[i].y + roi.y, w, h));

            // Dimension Error
            sizeWeights.push_back(1.f - std::fabs(r - std::sqrt(static_cast<float>(w * w + h * h))) / diagSize);
            // Distance Error
            locationWeights.push_back(1.f - std::sqrt(static_cast<float>((x - clickPoint.x) * (x - clickPoint.x) + (y - clickPoint.y) * (y - clickPoint.y))) / r);
            // General Error
            combinedWeights.push_back(locationWeightContribFactor * locationWeights[counter] + sizeWeightContribFactor * sizeWeights[counter]);
            if(combinedWeights[bestIdx] < combinedWeights[counter])
            {
                bestIdx = counter;
            }


            counter++;
        }
    }

    // Check if there is no boundingbox is valid
    if(finalRect.empty())
    {
        _finalRect.push_back(cv::Rect(clickPoint.x - initSize.width / 2, clickPoint.y - initSize.height / 2, initSize.width, initSize.height));
        return _finalRect;
    }

    // Return the final result
    _finalRect.push_back(finalRect[bestIdx]);
    return _finalRect;
}

void GSauvola::binarize(const cv::Mat input, cv::Mat &output, const int winSize, const bool padding)
{
    if(input.channels() > 1)
    {
        std::cerr << "ERROR: Invalid input image.\r\n";
        return;
    }
    int paddingSize = 0;
    if(padding)
    {
        paddingSize = winSize / 2;
    }
    cv::Mat paddedImg;
    pad(input, paddedImg, paddingSize, 127);
    int height      = paddedImg.rows;
    int width       = paddedImg.cols;
    output          = cv::Mat(input.size(), CV_8UC1, 255);
    cv::Mat thres   = cv::Mat::zeros(input.size(), CV_32FC1);
    // === hainh35
//    cv::parallel_for_(cv::Range(0, (width - winSize + 1) * (height - winSize + 1)),
//                      [&](const cv::Range &range)
//    {
//        for(int k = range.start; k < range.end; k++)
//        {
//            int i = k / (height - winSize + 1);
//            int j = k % (height - winSize + 1);
//            cv::Scalar mean, stddev;
//            cv::Rect roi(i, j, winSize, winSize);
//            cv::meanStdDev(paddedImg(roi), mean, stddev);
//            if(stddev[0] < 6.0)
//                thres.at<float>(j, i) = float(0.8 * (mean[0] + 2.0 * stddev[0]));
//            else
//                thres.at<float>(j, i) = float(mean[0] * (1. + stddev[0] / 1024.));
//        }
//    });
    cv::Range range(0, (width - winSize + 1) * (height - winSize + 1));
    for(int k = range.start; k < range.end; k++)
    {
        int i = k / (height - winSize + 1);
        int j = k % (height - winSize + 1);
        cv::Scalar mean, stddev;
        cv::Rect roi(i, j, winSize, winSize);
        cv::meanStdDev(paddedImg(roi), mean, stddev);
        if(stddev[0] < 6.0)
            thres.at<float>(j, i) = float(0.8 * (mean[0] + 2.0 * stddev[0]));
        else
            thres.at<float>(j, i) = float(mean[0] * (1. + stddev[0] / 1024.));
    }
    // hainh35 ===
    cv::Mat temp;
    paddedImg(cv::Rect(paddingSize, paddingSize, input.cols, input.rows)).copyTo(temp);
    temp.convertTo(temp, CV_32FC1);
    cv::compare(temp, thres, output, cv::CMP_GE);
}

void GSauvola::pad(const cv::Mat input, cv::Mat &output, const int paddingSize, const int paddingValue)
{
    int width   = input.cols;
    int height  = input.rows;

    output = cv::Mat(height + 2 * paddingSize, width + 2 * paddingSize, input.type(), paddingValue);
    cv::Rect roi(paddingSize, paddingSize, width, height);

    input.copyTo(output(roi));
}

//=================================================================================================

#include <fstream>

#define NUM_COLOR   11

CNExtractor::CNExtractor()
{
    loadCNModel();

    // order of color names: black, blue, brown, gray, green, orange, pink, purple, red, white, yellow
    colorVec = {{0, 0, 0}, {0, 0, 1}, {0.5, 0.4f, 0.25},
                                                {0.5, 0.5, 0.5}, {0, 1, 0}, {1, 0.8f, 0},
                                                {1, 0.5, 1}, {1, 0, 1}, {1, 0, 0},
                                                {1, 1, 1}, {1, 1, 0}};
}

void CNExtractor::loadCNModel()
{
    std::ifstream w2cFile;
    w2cFile.open("/home/giapvn/workspaces/Algorithm/Binarization/G-Sauvola/w2c.txt");

    if(!w2cFile)
    {
        std::cerr << "ERROR: Unable to open file w2c.txt\r\n";
        exit(1);
    }

    cv::Mat w2cMat(0, NUM_COLOR + 3, cv::DataType<float>::type);
    std::string delimiter = " ";

    for(std::string line; getline(w2cFile, line);)
    {
        std::vector<float> lineVec;
        size_t pos = 0;
        while((pos = line.find(delimiter)) != std::string::npos)
        {
            lineVec.push_back(std::stof(line.substr(0, pos)));
            line.erase(0, pos + delimiter.length());
        }
        if(lineVec.size() != NUM_COLOR + 3)
        {
            std::cerr << "ERROR: Invalid Color Name Model: Check in line \"" << line<< "\"\r\n";
            exit(1);
        }
        cv::Mat v2m(1, NUM_COLOR + 3, cv::DataType<float>::type, lineVec.data());
        w2cMat.push_back(v2m);
    }

    m_cnModel = w2cMat(cv::Rect(3, 0, NUM_COLOR, w2cMat.rows));
    std::cout << m_cnModel.size() << std::endl;
}

cv::Mat CNExtractor::divideByNumber(cv::Mat inMat, int number)
{
    if(number == 0)
    {
        std::cerr << "ERROR: Cannot divide by 0!\r\n";
        return cv::Mat();
    }

    cv::Mat outMat = cv::Mat::zeros(inMat.size(), inMat.type());
    for(int i = 0; i < outMat.rows; i++)
        for(int j = 0; j < outMat.cols; j++)
            outMat.at<uchar>(i, j) = inMat.at<uchar>(i, j) / static_cast<uchar>(number);

    return outMat;
}

void CNExtractor::extractCN(const cv::Mat input, cv::Mat& output)
{
    cv::Mat channels[3], bChannel, gChannel, rChannel;
    cv::split(input, channels);

    bChannel = divideByNumber(channels[0], 8);
    gChannel = divideByNumber(channels[1], 8);
    rChannel = divideByNumber(channels[2], 8);

    bChannel.convertTo(bChannel, CV_32SC1);
    gChannel.convertTo(gChannel, CV_32SC1);
    rChannel.convertTo(rChannel, CV_32SC1);

    cv::Mat indexImg = rChannel + 32 * gChannel + 32 * 32 * bChannel;
    cv::Mat indexVec = indexImg.reshape(0, 1);  // a matrix 1xnx1

    // Get index of maximum element of each row of m_cnModel
    std::vector<int> maxIdx;
    cv::Mat row;
    double maxVal, minVal;
    cv::Point minLoc, maxLoc;
    cv::minMaxLoc(indexVec, &minVal, &maxVal, &minLoc, &maxLoc);
    std::cout << maxVal << std::endl;
    for(int i = 0; i < m_cnModel.rows; i++)
    {
        row = m_cnModel.row(i);
        cv::minMaxLoc(row, &minVal, &maxVal, &minLoc, &maxLoc);
        maxIdx.push_back(maxLoc.x);
    }

    std::vector<int> out;
    for(int i = 0; i < indexVec.cols; i++)
    {
        out.push_back(maxIdx[static_cast<unsigned long>(indexVec.at<int>(0, i))]);
    }

    cv::Mat outMat(1, static_cast<int>(out.size()), cv::DataType<int>::type, out.data());

    outMat = outMat.reshape(0, input.cols);
//    cv::Mat outMatT = outMat;

    output = cv::Mat::zeros(input.size(), input.type());
    for(int i = 0; i < output.rows; i++)
        for(int j = 0; j < output.cols; j++)
        {
            channels[0].at<uchar>(i, j) = static_cast<uchar>(std::floor(colorVec[outMat.at<int>(i, j)][2] * 255));
            channels[1].at<uchar>(i, j) = static_cast<uchar>(std::floor(colorVec[outMat.at<int>(i, j)][1] * 255));
            channels[2].at<uchar>(i, j) = static_cast<uchar>(std::floor(colorVec[outMat.at<int>(i, j)][0] * 255));
        }
    cv::merge(channels, 3, output);
//    cv::imshow("test", output);
//    cv::waitKey(0);
}
